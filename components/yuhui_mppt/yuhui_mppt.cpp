#include "yuhui_mppt.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include <cstdio>
#include <ctime>

namespace esphome {
namespace yuhui {

static const char *const TAG = "yuhui_mppt";

/// 协议全部为「高字节在前」的大端序
static inline uint16_t be16(const uint8_t *p) {
  return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}
static inline uint32_t be32(const uint8_t *p) {
  return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

void YuhuiMPPT::setup() {
  // 配了只有 0xB1 才有的字段时，自动改成全量轮询，免得传感器永远不更新
  if (this->rated_voltage_sensor_ != nullptr || this->max_charge_current_sensor_ != nullptr ||
      this->battery_type_text_sensor_ != nullptr) {
    if (!this->poll_full_status_) {
      ESP_LOGI(TAG, "Extended sensors configured, switching to 0xB1 full polling");
      this->poll_full_status_ = true;
    }
  }
}

void YuhuiMPPT::update() {
  // 协议要求上位机查询间隔 >= 1s，这里由 update_interval 保证
  this->enqueue_(this->build_query_(this->poll_full_status_ ? CMD_QUERY_ALL : CMD_QUERY_REALTIME), true);
}

void YuhuiMPPT::dump_config() {
  ESP_LOGCONFIG(TAG, "Yuhui MPPT:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->get_address());
  ESP_LOGCONFIG(TAG, "  RX timeout: %u ms", this->rx_timeout_);
  ESP_LOGCONFIG(TAG, "  Retries per command: %u", this->retry_count_);
  ESP_LOGCONFIG(TAG, "  Poll 0xB1 full status: %s", YESNO(this->poll_full_status_));
  ESP_LOGCONFIG(TAG, "  Clock calibration broadcast: %s", YESNO(this->clock_broadcast_));
  ESP_LOGCONFIG(TAG, "  Update interval: %u ms", this->get_update_interval());
}

void YuhuiMPPT::loop() {
  if (this->awaiting_response_) {
    const uint32_t now = millis();
    if (now - this->last_send_time_ < this->rx_timeout_)
      return;

    this->awaiting_response_ = false;

    if (this->retries_left_ > 0) {
      this->retries_left_--;
      ESP_LOGW(TAG, "No response in %u ms, retrying (left: %u)", this->rx_timeout_, this->retries_left_);
      this->send_pending_();
      return;
    }

    const uint8_t command = this->pending_payload_.size() > 1 ? this->pending_payload_[1] : 0;
    ESP_LOGW(TAG, "Command 0x%02X timed out", command);
    this->pending_payload_.clear();
    if (++this->consecutive_failures_ >= 3) {
      ESP_LOGE(TAG, "%u commands in a row got no response, check wiring / address / baud rate",
               this->consecutive_failures_);
      this->status_set_warning();
    }
  }

  this->send_next_();
}

void YuhuiMPPT::send_next_() {
  if (this->awaiting_response_ || this->queue_.empty())
    return;

  PendingCommand command = this->queue_.front();
  this->queue_.pop();

  this->pending_payload_ = command.payload;
  this->retries_left_ = this->retry_count_;
  this->write_frame(this->pending_payload_);

  if (command.expect_reply) {
    this->awaiting_response_ = true;
    this->last_send_time_ = millis();
  } else {
    // 群控命令没有应答，发完就当完成，下一个 loop 继续发队列里的下一条
    this->pending_payload_.clear();
  }
}

void YuhuiMPPT::send_pending_() {
  if (this->pending_payload_.empty())
    return;
  this->write_frame(this->pending_payload_);
  this->awaiting_response_ = true;
  this->last_send_time_ = millis();
}

void YuhuiMPPT::finish_command_() {
  this->awaiting_response_ = false;
  this->retries_left_ = 0;
  this->consecutive_failures_ = 0;
  this->pending_payload_.clear();
}

void YuhuiMPPT::enqueue_(std::vector<uint8_t> payload, bool expect_reply) {
  if (this->queue_.size() >= MAX_QUEUE_SIZE) {
    // 设备离线时反复点按钮会一直堆内存，丢掉最老的一条
    ESP_LOGW(TAG, "Command queue full, dropping oldest entry");
    this->queue_.pop();
  }
  this->queue_.push(PendingCommand{payload, expect_reply});
}

std::vector<uint8_t> YuhuiMPPT::build_query_(uint8_t command) const {
  // 地址 + 命令 + 控制码(0x01) + 4 字节填充，校验码由传输层补
  return {this->get_address(), command, 0x01, 0x00, 0x00, 0x00, 0x00};
}

std::vector<uint8_t> YuhuiMPPT::build_command_(uint8_t command, uint8_t code, uint32_t value,
                                              uint8_t data_length) const {
  std::vector<uint8_t> data = {this->get_address(), command, code, 0x00, 0x00, 0x00, 0x00};
  // 1 字节数据 -> 数据4(下标 6)；2 字节 -> 数据3,4；4 字节 -> 数据1~4；都是高字节在前
  switch (data_length) {
    case 1:
      data[6] = static_cast<uint8_t>(value & 0xFF);
      break;
    case 2:
      data[5] = static_cast<uint8_t>((value >> 8) & 0xFF);
      data[6] = static_cast<uint8_t>(value & 0xFF);
      break;
    case 4:
      data[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
      data[4] = static_cast<uint8_t>((value >> 16) & 0xFF);
      data[5] = static_cast<uint8_t>((value >> 8) & 0xFF);
      data[6] = static_cast<uint8_t>(value & 0xFF);
      break;
    default:
      break;
  }
  return data;
}

void YuhuiMPPT::send_control_command(uint8_t control_code) {
  this->enqueue_(this->build_command_(CMD_CONTROL, control_code, 0, 0), true);
}

void YuhuiMPPT::send_parameter_command(uint8_t parameter_code, uint32_t value, uint8_t data_length) {
  if (data_length != 1 && data_length != 2 && data_length != 4) {
    ESP_LOGE(TAG, "Invalid parameter length %u (must be 1, 2 or 4)", data_length);
    return;
  }
  this->enqueue_(this->build_command_(CMD_SET_PARAMETER, parameter_code, value, data_length), true);
}

void YuhuiMPPT::send_clock_calibration_command() {
  const time_t now = ::time(nullptr);
  // SNTP 没同步时 localtime 给出的是 1970 年，别把 MPPT 时钟对到 1970
  if (now < 1700000000) {
    ESP_LOGW(TAG, "System time not synced yet, clock calibration skipped");
    return;
  }

  struct tm *timeinfo = ::localtime(&now);
  if (timeinfo == nullptr) {
    ESP_LOGW(TAG, "localtime() failed, clock calibration skipped");
    return;
  }
  const struct tm tm_value = *timeinfo;  // localtime 返回的是静态缓冲区，先拷出来

  std::vector<uint8_t> data = {
      static_cast<uint8_t>(this->clock_broadcast_ ? 0x00 : this->get_address()),
      CMD_SET_CLOCK,
      static_cast<uint8_t>((tm_value.tm_year + 1900) % 100),
      static_cast<uint8_t>(tm_value.tm_mon + 1),
      static_cast<uint8_t>(tm_value.tm_mday),
      static_cast<uint8_t>(tm_value.tm_hour),
      static_cast<uint8_t>(tm_value.tm_min),
  };

  // 地址 0x00 是群控，协议规定不返回数据
  this->enqueue_(data, !this->clock_broadcast_);
}

void YuhuiMPPT::send_baud_rate_command(uint8_t baud_code) {
  if (baud_code < 1 || baud_code > 4) {
    ESP_LOGE(TAG, "Invalid baud code %u (1=1200 2=2400 3=4800 4=9600)", baud_code);
    return;
  }
  ESP_LOGW(TAG, "Changing baud rate: the MPPT will stop answering on the old rate!");
  // 广播地址 0x00，控制码固定 0x42，无应答
  std::vector<uint8_t> data = {0x00, CMD_SET_BAUD, 0x42, baud_code, 0x00, 0x00, 0x00};
  this->enqueue_(data, false);
}

void YuhuiMPPT::on_modbus_frame(const uint8_t *data, size_t len) {
  if (len < 8) {
    ESP_LOGW(TAG, "Frame too short (%u bytes)", static_cast<unsigned>(len));
    return;
  }

  const uint8_t command = data[1];

  if (command == CMD_ERROR) {
    this->handle_error_frame_(data);
    this->finish_command_();
    return;
  }

  switch (command) {
    case CMD_QUERY_REALTIME:
      if (len != 37) {
        ESP_LOGW(TAG, "0xB3 frame should be 37 bytes, got %u", static_cast<unsigned>(len));
        this->finish_command_();
        return;
      }
      // 0xB3：实时数值从 Byte6 开始，电量从 Byte20 开始
      this->parse_status_block_(data, 6, 20);
      break;

    case CMD_QUERY_ALL:
      if (len != 93) {
        ESP_LOGW(TAG, "0xB1 frame should be 93 bytes, got %u", static_cast<unsigned>(len));
        this->finish_command_();
        return;
      }
      // 0xB1：实时数值从 Byte30 开始，电量从 Byte44 开始
      this->parse_status_block_(data, 30, 44);
      this->parse_full_frame_(data);
      break;

    case CMD_QUERY_SETTINGS:
      if (len != 64) {
        ESP_LOGW(TAG, "0xB2 frame should be 64 bytes, got %u", static_cast<unsigned>(len));
        this->finish_command_();
        return;
      }
      this->parse_settings_frame_(data);
      break;

    case CMD_CONTROL:
    case CMD_SET_PARAMETER:
    case CMD_SET_CLOCK:
      // 协议规定这三类命令把收到的数据原样回抛，收到就算成功
      ESP_LOGD(TAG, "Command 0x%02X acknowledged by device", command);
      break;

    default:
      ESP_LOGW(TAG, "Unexpected command 0x%02X", command);
      this->finish_command_();
      return;
  }

  this->status_clear_error();
  this->status_clear_warning();
  this->finish_command_();
}

void YuhuiMPPT::handle_error_frame_(const uint8_t *data) {
  const uint8_t error_code = data[2];
  const uint8_t original_command = data[3];
  const uint8_t original_code = data[4];

  const char *text = "Unknown error";
  switch (error_code) {
    case 0x01:
      text = "Operation not allowed in current state";
      break;
    case 0x02:
      text = "Unsupported parameter code";
      break;
    case 0x03:
      text = "Parameter out of range";
      break;
    default:
      break;
  }

  ESP_LOGE(TAG, "MPPT error 0x%02X (%s) for command 0x%02X / code 0x%02X", error_code, text, original_command,
           original_code);
  this->status_set_error();

  if (this->error_text_sensor_ != nullptr) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%s (0x%02X)", text, error_code);
    this->error_text_sensor_->publish_state(buffer);
  }
}

void YuhuiMPPT::parse_status_block_(const uint8_t *data, size_t offset_values, size_t offset_energy) {
  const uint8_t status = data[3];
  const uint8_t charging = data[4];
  const uint8_t control = data[5];

  // ---- 运行状态 Byte3 ----
  if (this->running_status_text_sensor_ != nullptr)
    this->running_status_text_sensor_->publish_state((status & 0x01) ? "Error" : "Normal");
  if (this->battery_status_text_sensor_ != nullptr)
    this->battery_status_text_sensor_->publish_state((status & 0x02) ? "Over-discharge protection" : "Normal");
  if (this->fan_status_text_sensor_ != nullptr)
    this->fan_status_text_sensor_->publish_state((status & 0x04) ? "Fan fault" : "Normal");
  if (this->temperature_status_text_sensor_ != nullptr)
    this->temperature_status_text_sensor_->publish_state((status & 0x08) ? "Over-temperature protection" : "Normal");
  if (this->dc_output_status_text_sensor_ != nullptr)
    this->dc_output_status_text_sensor_->publish_state((status & 0x10) ? "DC output short-circuit protection"
                                                                      : "Normal");
  if (this->internal_temperature1_status_text_sensor_ != nullptr)
    this->internal_temperature1_status_text_sensor_->publish_state((status & 0x20) ? "Fault" : "Normal");
  if (this->internal_temperature2_status_text_sensor_ != nullptr)
    this->internal_temperature2_status_text_sensor_->publish_state((status & 0x40) ? "Fault" : "Normal");
  if (this->external_temperature1_status_text_sensor_ != nullptr)
    this->external_temperature1_status_text_sensor_->publish_state((status & 0x80) ? "Fault" : "Normal");

  // ---- 充电状态 Byte4 ----
  const bool remote_disable_charging = (charging & 0x40) != 0;
  if (this->charging_status_sensor_ != nullptr)
    this->charging_status_sensor_->publish_state((charging & 0x01) != 0);
  if (this->equalizing_charge_sensor_ != nullptr)
    this->equalizing_charge_sensor_->publish_state((charging & 0x02) != 0);
  if (this->tracking_sensor_ != nullptr)
    this->tracking_sensor_->publish_state((charging & 0x04) != 0);
  if (this->floating_charge_sensor_ != nullptr)
    this->floating_charge_sensor_->publish_state((charging & 0x08) != 0);
  if (this->charging_current_limit_sensor_ != nullptr)
    this->charging_current_limit_sensor_->publish_state((charging & 0x10) != 0);
  if (this->charging_derating_sensor_ != nullptr)
    this->charging_derating_sensor_->publish_state((charging & 0x20) != 0);
  if (this->remote_control_disable_charging_sensor_ != nullptr)
    this->remote_control_disable_charging_sensor_->publish_state(remote_disable_charging);
  if (this->pv_overvoltage_sensor_ != nullptr)
    this->pv_overvoltage_sensor_->publish_state((charging & 0x80) != 0);
  // 开关状态以设备回报为准，避免 UI 和实际不一致
  if (this->disable_charge_switch_ != nullptr)
    this->disable_charge_switch_->publish_state(remote_disable_charging);

  // ---- 控制状态 Byte5 ----
  const bool load_output = (control & 0x02) != 0;
  if (this->charging_relay_sensor_ != nullptr)
    this->charging_relay_sensor_->publish_state((control & 0x01) != 0);
  if (this->load_output_sensor_ != nullptr)
    this->load_output_sensor_->publish_state(load_output);
  if (this->fan_control_sensor_ != nullptr)
    this->fan_control_sensor_->publish_state((control & 0x04) != 0);
  if (this->overcharge_protection_sensor_ != nullptr)
    this->overcharge_protection_sensor_->publish_state((control & 0x10) != 0);
  if (this->overvoltage_protection_sensor_ != nullptr)
    this->overvoltage_protection_sensor_->publish_state((control & 0x20) != 0);
  if (this->dc_output_switch_ != nullptr)
    this->dc_output_switch_->publish_state(load_output);

  // ---- 实时数值 ----
  // 布局（相对 offset_values）：PV 电压(2) 电池电压(2) 充电电流(2) 内部温度1(2) 内部温度2(2) 外部温度1(2)
  const float pv_voltage = be16(data + offset_values) / 10.0f;          // 1 位小数
  const float battery_voltage = be16(data + offset_values + 2) / 100.0f;  // 2 位小数
  const float charging_current = be16(data + offset_values + 4) / 100.0f;
  const float internal_temperature = be16(data + offset_values + 6) / 10.0f;
  // data[offset_values + 8..9] 是内部温度2，协议已标注取消
  const float external_temperature = be16(data + offset_values + 10) / 10.0f;

  if (this->pv_voltage_sensor_ != nullptr)
    this->pv_voltage_sensor_->publish_state(pv_voltage);
  if (this->battery_voltage_sensor_ != nullptr)
    this->battery_voltage_sensor_->publish_state(battery_voltage);
  if (this->charging_current_sensor_ != nullptr)
    this->charging_current_sensor_->publish_state(charging_current);
  if (this->internal_temperature_sensor_ != nullptr)
    this->internal_temperature_sensor_->publish_state(internal_temperature);
  if (this->external_temperature_sensor_ != nullptr)
    this->external_temperature_sensor_->publish_state(external_temperature);

  // 用电池侧电压算充电功率（MPPT 输出功率）
  const float charging_power = battery_voltage * charging_current;
  if (this->charging_power_sensor_ != nullptr)
    this->charging_power_sensor_->publish_state(charging_power);

  // ---- 电量：协议单位为瓦时(Wh)，统一换算成 kWh ----
  const uint32_t daily_energy_wh = be32(data + offset_energy);
  const uint32_t total_energy_wh = be32(data + offset_energy + 4);
  // 无显示板的机型这两个字段无效，全 0xFF 时不上报
  if (daily_energy_wh != 0xFFFFFFFFu && this->daily_energy_sensor_ != nullptr)
    this->daily_energy_sensor_->publish_state(daily_energy_wh * 0.001f);
  if (total_energy_wh != 0xFFFFFFFFu && this->total_energy_sensor_ != nullptr)
    this->total_energy_sensor_->publish_state(total_energy_wh * 0.001f);

  ESP_LOGD(TAG, "PV %.1fV / Bat %.2fV / %.2fA / %.1fC / %.2fW / today %.3fkWh / total %.3fkWh", pv_voltage,
           battery_voltage, charging_current, internal_temperature, charging_power, daily_energy_wh * 0.001f,
           total_energy_wh * 0.001f);
}

void YuhuiMPPT::publish_battery_type_(uint8_t code) {
  if (this->battery_type_text_sensor_ == nullptr)
    return;
  switch (code) {
    case 0:
      this->battery_type_text_sensor_->publish_state("Lead-acid (AGM)");
      break;
    case 1:
      this->battery_type_text_sensor_->publish_state("Lead-acid (GEL)");
      break;
    case 2:
      this->battery_type_text_sensor_->publish_state("Lead-acid (Flooded)");
      break;
    case 3:
      this->battery_type_text_sensor_->publish_state("Lithium");
      break;
    default:
      this->battery_type_text_sensor_->publish_state("Unknown");
      break;
  }
}

void YuhuiMPPT::parse_full_frame_(const uint8_t *data) {
  // Byte8：电池类型
  this->publish_battery_type_(data[8]);

  if (this->rated_voltage_sensor_ != nullptr)
    this->rated_voltage_sensor_->publish_state(be16(data + 16) / 100.0f);
  if (this->max_charge_current_sensor_ != nullptr)
    this->max_charge_current_sensor_->publish_state(be16(data + 26) / 100.0f);

  ESP_LOGD(TAG, "0xB1: battery count %u, baud code %u, model 0x%02X, load mode %u", data[10], data[13], data[52],
           data[11]);
}

void YuhuiMPPT::parse_settings_frame_(const uint8_t *data) {
  // 0xB2 的字段位置和 0xB1 不同：电池类型 Byte3、额定电压 Byte9~10、最大充电电流 Byte19~20
  this->publish_battery_type_(data[3]);

  if (this->rated_voltage_sensor_ != nullptr)
    this->rated_voltage_sensor_->publish_state(be16(data + 9) / 100.0f);
  if (this->max_charge_current_sensor_ != nullptr)
    this->max_charge_current_sensor_->publish_state(be16(data + 19) / 100.0f);

  ESP_LOGD(TAG, "0xB2: identify mode %u, battery count %u, load mode %u, baud code %u", data[4], data[5], data[6],
           data[8]);
}

}  // namespace yuhui
}  // namespace esphome

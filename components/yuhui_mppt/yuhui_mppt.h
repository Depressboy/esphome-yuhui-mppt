#pragma once

#include "esphome/core/component.h"
#include "esphome/components/custom_modbus/modbus.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/switch/switch.h"

#include <queue>
#include <vector>

namespace esphome {
namespace yuhui {

/** 命令码（依据《MPPT远程通信协议 V1.0》）*/
enum CommandCode : uint8_t {
  CMD_QUERY_ALL = 0xB1,       // 全量查询，应答 93 字节
  CMD_QUERY_SETTINGS = 0xB2,  // 仅查询设置参数，应答 64 字节
  CMD_QUERY_REALTIME = 0xB3,  // 仅查询实时数据，应答 37 字节
  CMD_CONTROL = 0xC0,         // 控制命令，应答为原样回抛 8 字节
  CMD_SET_PARAMETER = 0xD0,   // 参数设置，应答为原样回抛 8 字节
  CMD_SET_BAUD = 0xDE,        // 设置波特率，广播地址 0x00，无应答
  CMD_SET_CLOCK = 0xDF,       // 设置时钟，地址 0x00 为群控且无应答
  CMD_ERROR = 0xEE,           // 设备错误帧，8 字节
};

/** 0xC0 控制码 */
enum ControlCode : uint8_t {
  CTRL_CHARGE_ENABLE = 0x01,
  CTRL_CHARGE_DISABLE = 0x02,
  CTRL_DC_OUTPUT_ON = 0x03,
  CTRL_DC_OUTPUT_OFF = 0x04,
  CTRL_SILENCE_ALARM = 0x05,
  CTRL_BACKLIGHT_ON = 0x06,
};

/** 0xD0 参数代码（节选，完整表见协议文档第五章）*/
enum ParameterCode : uint8_t {
  PARAM_BATTERY_TYPE = 0x09,        // 1 字节：0 铅酸免维护 / 1 铅酸胶体 / 2 铅酸液体 / 3 锂电
  PARAM_RATED_VOLTAGE = 0x0A,       // 1 字节：0 自动识别，1=12V 2=24V ...
  PARAM_LOAD_MODE = 0x0C,           // 1 字节：0 关闭 / 1 自动 / 2 时控 / 3 光控 / 4 远程
  PARAM_MODEL_CODE = 0x11,          // 1 字节
  PARAM_TIMER_GROUP_FLAGS = 0x12,   // 1 字节
  PARAM_EQUALIZE_VOLTAGE = 0x21,    // 2 字节，两位小数
  PARAM_FLOAT_VOLTAGE = 0x22,       // 2 字节，两位小数
  PARAM_LOW_VOLTAGE_PROTECT = 0x23, // 2 字节，两位小数
  PARAM_MAX_CHARGE_CURRENT = 0x25,  // 2 字节，两位小数
  PARAM_LOW_VOLTAGE_RECOVER = 0x26, // 2 字节，两位小数
  PARAM_OVERVOLTAGE_PROTECT = 0x27, // 2 字节，两位小数
  PARAM_OVERVOLTAGE_RECOVER = 0x28, // 2 字节，两位小数
  PARAM_LIGHT_ON_VOLTAGE = 0x29,    // 2 字节，无小数，最大 999
  PARAM_LIGHT_OFF_VOLTAGE = 0x2A,   // 2 字节，无小数
  PARAM_LIGHT_ON_DELAY = 0x2B,      // 2 字节，秒
  PARAM_LIGHT_OFF_DELAY = 0x2C,     // 2 字节，秒
};

/** 命令码 -> 整帧长度（含地址与校验码），0 表示未知命令 */
inline size_t frame_length_for_command(uint8_t command) {
  switch (command) {
    case CMD_QUERY_ALL:
      return 93;
    case CMD_QUERY_SETTINGS:
      return 64;
    case CMD_QUERY_REALTIME:
      return 37;
    case CMD_CONTROL:
    case CMD_SET_PARAMETER:
    case CMD_SET_CLOCK:
    case CMD_ERROR:
      return 8;
    default:
      return 0;  // 0xDE 无应答，不需要长度
  }
}

class YuhuiMPPT : public PollingComponent, public custom_modbus::ModbusDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  // ---- 配置项 ----
  void set_device_address(uint8_t address) { this->set_address(address); }
  void set_rx_timeout(uint32_t timeout) { this->rx_timeout_ = timeout; }
  void set_retry_count(uint8_t retry_count) { this->retry_count_ = retry_count; }
  /** 是否每次轮询发 0xB1（93 字节全量帧）。配上额外传感器后会自动打开。 */
  void set_poll_full_status(bool full) { this->poll_full_status_ = full; }
  /** 对时是否使用群控地址 0x00（不返回数据）。 */
  void set_clock_broadcast(bool broadcast) { this->clock_broadcast_ = broadcast; }

  // ---- 传输层回调 ----
  size_t frame_length_for(uint8_t command) const override { return frame_length_for_command(command); }
  void on_modbus_frame(const uint8_t *data, size_t len) override;

  // ---- 主动命令 ----
  void send_query_command() { this->query_realtime(); }
  void query_realtime() { this->enqueue_(this->build_query_(CMD_QUERY_REALTIME), true); }
  void query_settings() { this->enqueue_(this->build_query_(CMD_QUERY_SETTINGS), true); }
  void query_all() { this->enqueue_(this->build_query_(CMD_QUERY_ALL), true); }

  void send_control_command(uint8_t control_code);
  void send_parameter_command(uint8_t parameter_code, uint32_t value, uint8_t data_length);
  void send_clock_calibration_command();
  /** 危险：改完波特率本机就联系不上了，需要在 MPPT 面板上同步修改。 */
  void send_baud_rate_command(uint8_t baud_code);

  // ---- 传感器注册 ----
  void set_running_status_text_sensor(text_sensor::TextSensor *sensor) { running_status_text_sensor_ = sensor; }
  void set_battery_status_text_sensor(text_sensor::TextSensor *sensor) { battery_status_text_sensor_ = sensor; }
  void set_fan_status_text_sensor(text_sensor::TextSensor *sensor) { fan_status_text_sensor_ = sensor; }
  void set_temperature_status_text_sensor(text_sensor::TextSensor *sensor) { temperature_status_text_sensor_ = sensor; }
  void set_dc_output_status_text_sensor(text_sensor::TextSensor *sensor) { dc_output_status_text_sensor_ = sensor; }
  void set_internal_temperature1_status_text_sensor(text_sensor::TextSensor *sensor) { internal_temperature1_status_text_sensor_ = sensor; }
  void set_internal_temperature2_status_text_sensor(text_sensor::TextSensor *sensor) { internal_temperature2_status_text_sensor_ = sensor; }
  void set_external_temperature1_status_text_sensor(text_sensor::TextSensor *sensor) { external_temperature1_status_text_sensor_ = sensor; }
  void set_battery_type_text_sensor(text_sensor::TextSensor *sensor) { battery_type_text_sensor_ = sensor; }
  void set_error_text_sensor(text_sensor::TextSensor *sensor) { error_text_sensor_ = sensor; }

  void set_charging_status_sensor(binary_sensor::BinarySensor *sensor) { charging_status_sensor_ = sensor; }
  void set_equalizing_charge_sensor(binary_sensor::BinarySensor *sensor) { equalizing_charge_sensor_ = sensor; }
  void set_tracking_sensor(binary_sensor::BinarySensor *sensor) { tracking_sensor_ = sensor; }
  void set_floating_charge_sensor(binary_sensor::BinarySensor *sensor) { floating_charge_sensor_ = sensor; }
  void set_charging_current_limit_sensor(binary_sensor::BinarySensor *sensor) { charging_current_limit_sensor_ = sensor; }
  void set_charging_derating_sensor(binary_sensor::BinarySensor *sensor) { charging_derating_sensor_ = sensor; }
  void set_remote_control_disable_charging_sensor(binary_sensor::BinarySensor *sensor) { remote_control_disable_charging_sensor_ = sensor; }
  void set_pv_overvoltage_sensor(binary_sensor::BinarySensor *sensor) { pv_overvoltage_sensor_ = sensor; }
  void set_charging_relay_sensor(binary_sensor::BinarySensor *sensor) { charging_relay_sensor_ = sensor; }
  void set_load_output_sensor(binary_sensor::BinarySensor *sensor) { load_output_sensor_ = sensor; }
  void set_fan_control_sensor(binary_sensor::BinarySensor *sensor) { fan_control_sensor_ = sensor; }
  void set_overcharge_protection_sensor(binary_sensor::BinarySensor *sensor) { overcharge_protection_sensor_ = sensor; }
  void set_overvoltage_protection_sensor(binary_sensor::BinarySensor *sensor) { overvoltage_protection_sensor_ = sensor; }

  void set_pv_voltage_sensor(sensor::Sensor *sensor) { pv_voltage_sensor_ = sensor; }
  void set_battery_voltage_sensor(sensor::Sensor *sensor) { battery_voltage_sensor_ = sensor; }
  void set_charging_current_sensor(sensor::Sensor *sensor) { charging_current_sensor_ = sensor; }
  void set_internal_temperature_sensor(sensor::Sensor *sensor) { internal_temperature_sensor_ = sensor; }
  void set_external_temperature_sensor(sensor::Sensor *sensor) { external_temperature_sensor_ = sensor; }
  void set_daily_energy_sensor(sensor::Sensor *sensor) { daily_energy_sensor_ = sensor; }
  void set_total_energy_sensor(sensor::Sensor *sensor) { total_energy_sensor_ = sensor; }
  void set_charging_power_sensor(sensor::Sensor *sensor) { charging_power_sensor_ = sensor; }
  void set_rated_voltage_sensor(sensor::Sensor *sensor) { rated_voltage_sensor_ = sensor; }
  void set_max_charge_current_sensor(sensor::Sensor *sensor) { max_charge_current_sensor_ = sensor; }

  void set_silence_alarm_button(button::Button *button) { silence_alarm_button_ = button; }
  void set_enable_backlight_button(button::Button *button) { enable_backlight_button_ = button; }
  void set_clock_calibration_button(button::Button *button) { clock_calibration_button_ = button; }

  void set_disable_charge_switch(esphome::switch_::Switch *sw) { disable_charge_switch_ = sw; }
  void set_dc_output_switch(esphome::switch_::Switch *sw) { dc_output_switch_ = sw; }

 protected:
  /** 待发送命令。expect_reply=false 用于群控命令（0xDE / 群控对时）。 */
  struct PendingCommand {
    std::vector<uint8_t> payload;
    bool expect_reply;
  };

  std::vector<uint8_t> build_query_(uint8_t command) const;
  std::vector<uint8_t> build_command_(uint8_t command, uint8_t code, uint32_t value, uint8_t data_length) const;
  void enqueue_(std::vector<uint8_t> payload, bool expect_reply);
  void send_next_();
  void send_pending_();
  void finish_command_();

  void parse_status_block_(const uint8_t *data, size_t offset_values, size_t offset_energy);
  void parse_full_frame_(const uint8_t *data);
  void parse_settings_frame_(const uint8_t *data);
  void publish_battery_type_(uint8_t code);
  void handle_error_frame_(const uint8_t *data);

  // ---- 文本传感器 ----
  text_sensor::TextSensor *running_status_text_sensor_{nullptr};
  text_sensor::TextSensor *battery_status_text_sensor_{nullptr};
  text_sensor::TextSensor *fan_status_text_sensor_{nullptr};
  text_sensor::TextSensor *temperature_status_text_sensor_{nullptr};
  text_sensor::TextSensor *dc_output_status_text_sensor_{nullptr};
  text_sensor::TextSensor *internal_temperature1_status_text_sensor_{nullptr};
  text_sensor::TextSensor *internal_temperature2_status_text_sensor_{nullptr};
  text_sensor::TextSensor *external_temperature1_status_text_sensor_{nullptr};
  text_sensor::TextSensor *battery_type_text_sensor_{nullptr};
  text_sensor::TextSensor *error_text_sensor_{nullptr};

  // ---- 二进制传感器 ----
  binary_sensor::BinarySensor *charging_status_sensor_{nullptr};
  binary_sensor::BinarySensor *equalizing_charge_sensor_{nullptr};
  binary_sensor::BinarySensor *tracking_sensor_{nullptr};
  binary_sensor::BinarySensor *floating_charge_sensor_{nullptr};
  binary_sensor::BinarySensor *charging_current_limit_sensor_{nullptr};
  binary_sensor::BinarySensor *charging_derating_sensor_{nullptr};
  binary_sensor::BinarySensor *remote_control_disable_charging_sensor_{nullptr};
  binary_sensor::BinarySensor *pv_overvoltage_sensor_{nullptr};
  binary_sensor::BinarySensor *charging_relay_sensor_{nullptr};
  binary_sensor::BinarySensor *load_output_sensor_{nullptr};
  binary_sensor::BinarySensor *fan_control_sensor_{nullptr};
  binary_sensor::BinarySensor *overcharge_protection_sensor_{nullptr};
  binary_sensor::BinarySensor *overvoltage_protection_sensor_{nullptr};

  // ---- 数值传感器 ----
  sensor::Sensor *pv_voltage_sensor_{nullptr};
  sensor::Sensor *battery_voltage_sensor_{nullptr};
  sensor::Sensor *charging_current_sensor_{nullptr};
  sensor::Sensor *internal_temperature_sensor_{nullptr};
  sensor::Sensor *external_temperature_sensor_{nullptr};
  sensor::Sensor *daily_energy_sensor_{nullptr};
  sensor::Sensor *total_energy_sensor_{nullptr};
  sensor::Sensor *charging_power_sensor_{nullptr};
  sensor::Sensor *rated_voltage_sensor_{nullptr};
  sensor::Sensor *max_charge_current_sensor_{nullptr};

  // ---- 实体 ----
  button::Button *silence_alarm_button_{nullptr};
  button::Button *enable_backlight_button_{nullptr};
  button::Button *clock_calibration_button_{nullptr};
  esphome::switch_::Switch *disable_charge_switch_{nullptr};
  esphome::switch_::Switch *dc_output_switch_{nullptr};

  // ---- 运行状态 ----
  uint32_t rx_timeout_{300};
  uint8_t retry_count_{2};
  bool poll_full_status_{false};
  bool clock_broadcast_{true};
  uint32_t last_send_time_{0};

  bool awaiting_response_{false};
  uint8_t retries_left_{0};
  uint8_t consecutive_failures_{0};
  std::vector<uint8_t> pending_payload_;

  static constexpr uint8_t MAX_QUEUE_SIZE = 8;
  std::queue<PendingCommand> queue_;
};

}  // namespace yuhui
}  // namespace esphome

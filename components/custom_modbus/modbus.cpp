#include "modbus.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include <cstdio>
#include <string>

namespace esphome {
namespace custom_modbus {

static const char *const TAG = "custom_modbus";

/// 自己拼十六进制串，不依赖 ESPHome 的 format_hex_pretty（不同版本位置变过）
static std::string hex_dump_(const std::vector<uint8_t> &data) {
  std::string out;
  out.reserve(data.size() * 3);
  char byte_str[8];
  for (uint8_t value : data) {
    snprintf(byte_str, sizeof(byte_str), "%02X", static_cast<unsigned>(value));
    if (!out.empty())
      out.push_back(' ');
    out += byte_str;
  }
  return out;
}

uint8_t Modbus::checksum(const uint8_t *data, size_t len) {
  uint16_t sum = 0;
  for (size_t i = 0; i < len; i++)
    sum += data[i];
  return static_cast<uint8_t>(sum & 0xFF);
}

void Modbus::setup() {
  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->setup();
    this->flow_control_pin_->digital_write(false);  // 默认接收态
  }
  this->rx_buffer_.reserve(128);
}

void Modbus::register_device(ModbusDevice *device) {
  device->parent_ = this;
  this->devices_.push_back(device);
}

float Modbus::get_setup_priority() const {
  // After UART bus
  return setup_priority::BUS - 1.0f;
}

bool Modbus::try_extract_frame_() {
  // 至少要有 地址 + 命令 两个字节才能判断这一帧有多长
  if (this->rx_buffer_.size() < 2)
    return false;

  const uint8_t address = this->rx_buffer_[0];
  const uint8_t command = this->rx_buffer_[1];

  size_t expected = 0;
  for (auto *device : this->devices_) {
    expected = device->frame_length_for(command);
    if (expected > 0)
      break;
  }

  // 不认识的命令：丢掉 1 个字节，往后找同步头
  if (expected == 0) {
    ESP_LOGV(TAG, "Resync: drop unknown command 0x%02X", command);
    this->rx_buffer_.erase(this->rx_buffer_.begin());
    return !this->rx_buffer_.empty();
  }

  if (this->rx_buffer_.size() < expected)
    return false;  // 还没收完

  const uint8_t received = this->rx_buffer_[expected - 1];
  const uint8_t calculated = Modbus::checksum(this->rx_buffer_.data(), expected - 1);
  if (received != calculated) {
    ESP_LOGW(TAG, "Checksum mismatch (got 0x%02X, want 0x%02X), resync", received, calculated);
    // 只丢 1 个字节：万一这帧是从中间开始对齐的，真帧头还在后面
    this->rx_buffer_.erase(this->rx_buffer_.begin());
    return !this->rx_buffer_.empty();
  }

  std::vector<uint8_t> frame(this->rx_buffer_.begin(), this->rx_buffer_.begin() + expected);
  this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + expected);
  this->waiting_for_response = 0;

  bool handled = false;
  for (auto *device : this->devices_) {
    if (address == 0x00 || address == device->get_address()) {
      device->on_modbus_frame(frame.data(), frame.size());
      handled = true;
    }
  }
  if (!handled) {
    ESP_LOGD(TAG, "Frame from unhandled address 0x%02X ignored", address);
  }

  return !this->rx_buffer_.empty();
}

void Modbus::loop() {
  const uint32_t now = millis();

  while (this->available()) {
    uint8_t byte;
    if (!this->read_byte(&byte))
      break;
    this->last_byte_ = now;
    this->rx_buffer_.push_back(byte);
    while (this->try_extract_frame_()) {
    }
  }

  // 半截帧（丢字节 / 噪声）闲置太久就清掉，避免下次拼接到新帧前面
  if (!this->rx_buffer_.empty() && (now - this->last_byte_) > this->frame_gap_) {
    ESP_LOGV(TAG, "Dropping %u bytes of incomplete frame", static_cast<unsigned>(this->rx_buffer_.size()));
    this->rx_buffer_.clear();
  }
}

void Modbus::write_frame(const std::vector<uint8_t> &payload) {
  if (payload.empty()) {
    ESP_LOGW(TAG, "Refusing to send empty frame");
    return;
  }

  std::vector<uint8_t> frame(payload);
  frame.push_back(Modbus::checksum(payload.data(), payload.size()));

  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(true);

  this->write_array(frame);
  this->flush();

  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(false);

  this->waiting_for_response = payload[0];
  this->last_send_ = millis();
  ESP_LOGD(TAG, "TX: %s", hex_dump_(frame).c_str());
}

void Modbus::dump_config() {
  ESP_LOGCONFIG(TAG, "Custom Modbus (Yuhui fixed-frame):");
  LOG_PIN("  Flow Control Pin: ", this->flow_control_pin_);
  ESP_LOGCONFIG(TAG, "  Frame gap: %u ms", this->frame_gap_);
  ESP_LOGCONFIG(TAG, "  Devices on bus: %u", static_cast<unsigned>(this->devices_.size()));
}

}  // namespace custom_modbus
}  // namespace esphome

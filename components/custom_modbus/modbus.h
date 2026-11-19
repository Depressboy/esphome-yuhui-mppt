#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

#include <vector>

namespace esphome {
namespace custom_modbus {

enum ModbusRole {
  CLIENT,
  SERVER,
};

class ModbusDevice;

/** 钰辉 MPPT 定长帧传输层。
 *
 * 注意：这**不是**标准 Modbus。
 *   - 每一帧长度固定，由命令码唯一决定；
 *   - 没有 CRC16，最后一字节是前面所有字节之和的低字节（累加和）；
 *   - 地址 0x00 为群控地址（只对 0xDE / 0xDF 有效，不回数据）。
 *
 * 接收端按「先看命令码算出整帧长度 -> 凑齐 -> 验累加和 -> 整帧交给设备」处理，
 * 校验失败只丢 1 个字节重新找同步头，所以中间丢字节也能自恢复。
 */
class Modbus : public uart::UARTDevice, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void register_device(ModbusDevice *device);

  /** 发送一帧，累加和自动补在末尾。payload 为不含校验码的数据。 */
  void write_frame(const std::vector<uint8_t> &payload);

  void set_role(ModbusRole role) { this->role_ = role; }
  void set_flow_control_pin(GPIOPin *flow_control_pin) { this->flow_control_pin_ = flow_control_pin; }
  void set_send_wait_time(uint16_t time_in_ms) { this->send_wait_time_ = time_in_ms; }
  void set_disable_crc(bool disable_crc) { this->disable_crc_ = disable_crc; }
  /** 半帧闲置超过该时间(ms)就当作残帧丢弃。9600bps 下 3.5 字符约 3.6ms。 */
  void set_frame_gap(uint16_t gap_ms) { this->frame_gap_ = gap_ms; }

  static uint8_t checksum(const uint8_t *data, size_t len);

  /** 最近一次发送的目标地址，0 表示空闲。仅用于诊断。 */
  uint8_t waiting_for_response{0};

 protected:
  /** 尝试从缓冲区里切出一整帧并分发；返回 true 表示缓冲区还有可处理的内容。 */
  bool try_extract_frame_();

  GPIOPin *flow_control_pin_{nullptr};
  // 下面三项是为兼容旧配置保留的（本协议没有 CRC16，也用不到 role / wait time）
  ModbusRole role_{ModbusRole::CLIENT};
  uint16_t send_wait_time_{250};
  bool disable_crc_{false};
  uint16_t frame_gap_{30};
  std::vector<uint8_t> rx_buffer_;
  uint32_t last_byte_{0};
  uint32_t last_send_{0};
  std::vector<ModbusDevice *> devices_;
};

class ModbusDevice {
 public:
  void set_parent(Modbus *parent) { this->parent_ = parent; }
  void set_address(uint8_t address) { this->address_ = address; }
  uint8_t get_address() const { return this->address_; }

  /** 该命令对应的整帧长度（含地址、命令、校验码）。返回 0 表示不认识该命令。 */
  virtual size_t frame_length_for(uint8_t command) const { return 0; }

  /** 收到一整帧且累加和已校验通过时调用。 */
  virtual void on_modbus_frame(const uint8_t *data, size_t len) {}

  /** 兼容旧接口，默认转发到 on_modbus_frame。 */
  virtual void on_modbus_data(const std::vector<uint8_t> &data) { this->on_modbus_frame(data.data(), data.size()); }

  void write_frame(const std::vector<uint8_t> &payload) { this->parent_->write_frame(payload); }

 protected:
  friend class Modbus;

  Modbus *parent_{nullptr};
  uint8_t address_{0x01};
};

}  // namespace custom_modbus
}  // namespace esphome

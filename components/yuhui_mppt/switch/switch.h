#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "../yuhui_mppt.h"

namespace esphome {
namespace yuhui {

class YuhuiMPPT;
class YuhuiMPPTSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(YuhuiMPPT *parent) { this->parent_ = parent; }
  void set_on_command(uint8_t command) { this->on_command_ = command; }
  void set_off_command(uint8_t command) { this->off_command_ = command; }

 protected:
  void write_state(bool state) override;
  uint8_t on_command_{0};
  uint8_t off_command_{0};
  YuhuiMPPT *parent_{nullptr};
};

}  // namespace yuhui
}  // namespace esphome

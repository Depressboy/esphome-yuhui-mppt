#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "../yuhui_mppt.h"

namespace esphome {
namespace yuhui {

class YuhuiMPPT;
class YuhuiMPPTButton : public button::Button, public Component {
 public:
  void set_parent(YuhuiMPPT *parent) { this->parent_ = parent; }
  void set_command(uint8_t command) { this->send_command_ = command; }
  void press_action() override;

 protected:
  YuhuiMPPT *parent_{nullptr};
  uint8_t send_command_{0};
};

class YuhuiMPPTClockCalibrationButton : public button::Button, public Component {
 public:
  void set_parent(YuhuiMPPT *parent) { this->parent_ = parent; }
  void press_action() override;

 protected:
  YuhuiMPPT *parent_{nullptr};
};

}  // namespace yuhui
}  // namespace esphome
#include "button.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace yuhui {

static const char *TAG = "yuhui_mppt.button";

void YuhuiMPPTButton::press_action() {
  ESP_LOGD(TAG, "Button pressed");
  this->parent_->send_control_command(this->send_command_);
}

void YuhuiMPPTClockCalibrationButton::press_action() {
  ESP_LOGD(TAG, "Clock calibration button pressed");
  this->parent_->send_clock_calibration_command();
}

}  // namespace yuhui
}  // namespace esphome
#include "switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace yuhui {

static const char *TAG = "yuhui_mppt.switch";

void YuhuiMPPTSwitch::write_state(bool state) {
  if (this->parent_ == nullptr) {
    ESP_LOGW(TAG, "No parent MPPT component, ignoring state change");
    return;
  }
  // 协议里没有"设置成功"的确认帧，只能原样回抛，所以先乐观更新 UI，
  // 下一轮查询拿到设备真实控制状态后会再纠正回来。
  this->parent_->send_control_command(state ? this->on_command_ : this->off_command_);
  this->publish_state(state);
}

}  // namespace yuhui
}  // namespace esphome

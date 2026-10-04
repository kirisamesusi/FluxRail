#pragma once
#include "../keyboard/KeyboardTypes.h"
#include "NormalHidCodec.h"
#include <Adafruit_TinyUSB.h>
#include <atomic>

#if !defined(FLUXRAIL_TINYUSB_STARTUP_ADAPTER) ||                              \
    FLUXRAIL_TINYUSB_STARTUP_ADAPTER < 2
#error                                                                         \
    "Build using dependencies/prepare_tinyusb.py and the project-owned TinyUSB 3.7.7 adapter"
#endif
namespace fluxrail {
namespace usb {
struct HidStats {
  uint32_t keyboard_submitted = 0, mouse_submitted = 0, completed = 0,
           failed = 0, busy = 0, bad_output = 0;
};
// 全部方法/回调只在核心0。唯一HID所有者，不使用库内keyboardPress/Mouse快捷状态。
class NormalHid : public keyboard::ActionSink {
public:
  bool
  configureBeforeConnect(); // 仅从库的启动钩子调用一次，setup前注册完整接口。
  bool connected() const;
  bool quiescent(); // 非阻塞：所有已提交释放报告完成后才允许Flash维护。
  bool detachIfQuiescent();
  bool connectionState(bool &connected) const;
  bool startupOk() const { return initialized_; }
  bool tryKeyboard(const common::KeyboardState &state) override;
  bool tryMouseButtons(uint8_t buttons) override;
  bool tryRelative(int16_t x, int16_t y);
  bool tryAbsolute(float output, int16_t y_counts = 0);
  // 仅采纳已暂停事务的活动模板；模式切换/维护不重新居中。
  bool setAbsoluteMode(bool enabled);
  // 故障/重连后只重发已提交的坐标，不重放相对增量；维护暂停时不发送。
  void requestAbsoluteRestore() {
    absolute_restore_pending_.store(true, std::memory_order_release);
  }
  void service(bool streaming);
  void reportCompleted(uint8_t instance, uint16_t bytes);
  void reportFailed(uint8_t instance);
  bool transportFaulted() const {
    return faulted_.load(std::memory_order_acquire);
  }
  void clearTransportFault() {
    faulted_.store(false, std::memory_order_release);
  }
  // 诊断也需避开USB IRQ，false表示本次暂忙，不返回撕裂的快照。
  bool readStats(HidStats &out) const;
  uint16_t getKeyboard(uint8_t id, hid_report_type_t type, uint8_t *buffer,
                       uint16_t requested);
  uint16_t getMouse(uint8_t id, hid_report_type_t type, uint8_t *buffer,
                    uint16_t requested);
  void keyboardOutput(uint8_t id, hid_report_type_t type, const uint8_t *buffer,
                      uint16_t bytes);

private:
  Adafruit_USBD_HID keyboard_{kKeyboardDescriptor, kKeyboardDescriptorBytes,
                              HID_ITF_PROTOCOL_NONE, 1, false};
  Adafruit_USBD_HID mouse_{kMouseDescriptor, kMouseDescriptorBytes,
                           HID_ITF_PROTOCOL_NONE, 1, false};
  common::KeyboardState keyboard_state_{};
  uint8_t buttons_ = 0, host_leds_ = 0;
  uint16_t absolute_x_ = kAbsoluteAxisCenter, absolute_y_ = kAbsoluteAxisCenter;
  bool initialized_ = false, absolute_mode_ = false, absolute_submitted_ = false;
  std::atomic<bool> absolute_restore_pending_{false};
  std::atomic<bool> faulted_{false};
  HidStats stats_{};
};
NormalHid &normalHid();
} // namespace usb
} // namespace fluxrail

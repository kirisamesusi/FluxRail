#include "app/ConfigCoordinator.h"
#include "realtime/InputWorker.h"
#include "usb/NormalHidCodec.h"
#include <stddef.h>
using namespace fluxrail;
extern "C" void *memset(void *d, int v, size_t n) {
  auto p = static_cast<unsigned char *>(d);
  for (size_t i = 0; i < n; ++i)
    p[i] = v;
  return d;
}
extern "C" void *memcpy(void *d, const void *s, size_t n) {
  auto p = static_cast<unsigned char *>(d);
  auto q = static_cast<const unsigned char *>(s);
  for (size_t i = 0; i < n; ++i)
    p[i] = q[i];
  return d;
}
void operator delete(void *) noexcept {}
void operator delete(void *, size_t) noexcept {}
extern "C" void __cxa_pure_virtual() {
  for (;;) {
  }
}
volatile uint32_t checks = 0;
#define CHECK(c)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(c))                                                                  \
      return __LINE__;                                                         \
  } while (0)
struct Sink : keyboard::ActionSink {
  common::KeyboardState keys[64]{};
  uint8_t mice[64]{};
  uint8_t kc = 0, mc = 0;
  bool blocked = false;
  bool tryKeyboard(const common::KeyboardState &s) override {
    if (blocked || kc == 64)
      return false;
    keys[kc++] = s;
    return true;
  }
  bool tryMouseButtons(uint8_t b) override {
    if (blocked || mc == 64)
      return false;
    mice[mc++] = b;
    return true;
  }
};
common::RuntimeConfig defaults() {
  common::RuntimeConfig c;
  c.keyboard = keyboard::defaultKeyboardConfig();
  return c;
}
void tick(realtime::InputWorker &worker, app::ConfigCoordinator &coordinator,
          Sink &sink, uint16_t raw, uint32_t &now) {
  now += 500;
  worker.service(raw, now);
  coordinator.service(now, sink);
}
void settle(realtime::InputWorker &worker, app::ConfigCoordinator &coordinator,
            Sink &sink, uint16_t raw, uint32_t &now) {
  for (uint8_t i = 0; i < 30 && coordinator.busy(); ++i)
    tick(worker, coordinator, sink, raw, now);
}
extern "C" int test_spsc_fifo_peek_full() {
  realtime::SpscQueue<common::InputEdge, 4> q;
  common::InputEdge out;
  CHECK(!q.tryPeek(out));
  CHECK(!q.pop());
  for (uint32_t round = 0; round < 100; ++round) {
    for (uint32_t i = 0; i < 4; ++i) {
      common::InputEdge e{round * 4 + i,           i,   7, 9,
                          static_cast<uint8_t>(i), true};
      CHECK(q.tryPush(e));
    }
    CHECK(!q.tryPush({999, 0, 0, 0, 0, false}));
    CHECK(q.tryPeek(out));
    CHECK(out.sequence == round * 4);
    CHECK(q.tryPeek(out));
    CHECK(out.sequence == round * 4);
    for (uint32_t i = 0; i < 4; ++i) {
      CHECK(q.tryPop(out));
      CHECK(out.sequence == round * 4 + i && out.revision == 7 &&
            out.epoch == 9);
    }
    CHECK(!q.tryPop(out));
  }
  CHECK(q.tryPush({1, 0, 0, 0, 0, false}));
  CHECK(q.tryPush({2, 0, 0, 0, 0, false}));
  CHECK(q.clearConsumer() == 2);
  CHECK(!q.pop());
  return 0;
}
extern "C" int test_mailbox_complete_value_copy() {
  realtime::ConfigMailbox mailbox;
  realtime::ConfigCommand command;
  command.epoch = 8;
  command.config = defaults();
  command.config.revision = 99;
  CHECK(mailbox.commands.tryPush(command));
  command.config.keyboard.bindings[7].usage = 0x2c;
  command.config.revision = 100;
  realtime::ConfigCommand received;
  CHECK(mailbox.commands.tryPop(received));
  CHECK(received.config.revision == 99 &&
        received.config.keyboard.bindings[7].usage == 4 && received.epoch == 8);
  common::InputSnapshot reply{99, 8, 123, 6, 0x0fff, true};
  CHECK(mailbox.replies.tryPush(reply));
  reply.physical_mask = 0;
  common::InputSnapshot out;
  CHECK(mailbox.replies.tryPop(out));
  CHECK(out.physical_mask == 0x0fff && out.ok);
  return 0;
}
extern "C" int test_pause_apply_ack_busy_and_resume() {
  realtime::RealtimeBridge bridge;
  realtime::InputWorker worker(bridge);
  keyboard::ActionMapper mapper;
  app::ConfigCoordinator coordinator(bridge, mapper);
  Sink sink;
  uint32_t now = 0;
  worker.begin(0, now);
  auto c = defaults();
  c.revision = 4;
  c.indicator_color = {20, 30, 40};
  CHECK(coordinator.start(c, now));
  sink.blocked = true;
  for (uint8_t i = 0; i < 12; ++i)
    tick(worker, coordinator, sink, 0, now);
  CHECK(!coordinator.ready() && coordinator.busy() && worker.paused());
  CHECK(sink.kc == 0 && mapper.recovering());
  CHECK(!coordinator.hasApplied() &&
        coordinator.appliedConfig().indicator_color.r == 255);
  // 暂停期间按下BT8再释放再按下，只恢复当前按下状态，不能补三段历史。
  for (uint8_t i = 0; i < 4; ++i)
    tick(worker, coordinator, sink, 1u << 7, now);
  for (uint8_t i = 0; i < 4; ++i)
    tick(worker, coordinator, sink, 0, now);
  for (uint8_t i = 0; i < 4; ++i)
    tick(worker, coordinator, sink, 1u << 7, now);
  sink.blocked = false;
  settle(worker, coordinator, sink, 1u << 7, now);
  CHECK(coordinator.ready() && !worker.paused());
  CHECK(coordinator.appliedConfig().revision == 4);
  CHECK(coordinator.appliedConfig().indicator_color.r == 20);
  CHECK(sink.kc == 2 && sink.mc == 2 && !sink.keys[1].held(4));
  common::InputEdge edge;
  CHECK(bridge.edges.tryPop(edge));
  CHECK(edge.source_id == 7 && edge.down && edge.sequence == 0);
  CHECK(edge.epoch == coordinator.epoch() && edge.revision == 4);
  CHECK(!bridge.edges.tryPeek(edge));
  CHECK(mapper.enqueue(edge.source_id, edge.down, edge.sequence,
                       edge.timestamp_us,
                       edge.revision) == keyboard::IngressResult::Accepted);
  mapper.service(now + 1000, sink);
  CHECK(sink.keys[2].held(4));
  return 0;
}
extern "C" int test_bt_push_single_stream() {
  realtime::RealtimeBridge bridge;
  realtime::InputWorker worker(bridge);
  keyboard::ActionMapper mapper;
  app::ConfigCoordinator coordinator(bridge, mapper);
  Sink sink;
  uint32_t now = 0;
  worker.begin(0, now);
  auto c = defaults();
  CHECK(coordinator.start(c, now));
  settle(worker, coordinator, sink, 0, now);
  CHECK(coordinator.ready());
  for (uint8_t i = 0; i < 4; ++i) {
    now += 500;
    worker.service(1, now);
  }
  CHECK(worker.submitPush(10, true, now));
  CHECK(!worker.submitPush(9, true, now));
  CHECK(!worker.submitPush(12, true, now));
  CHECK(worker.submitPush(10, false, now + 1));
  common::InputEdge edge;
  CHECK(bridge.edges.tryPop(edge));
  CHECK(edge.source_id == 0 && edge.down && edge.sequence == 0);
  CHECK(bridge.edges.tryPop(edge));
  CHECK(edge.source_id == 10 && edge.down && edge.sequence == 1);
  CHECK(bridge.edges.tryPop(edge));
  CHECK(edge.source_id == 10 && !edge.down && edge.sequence == 2);
  CHECK(!bridge.edges.tryPop(edge));
  return 0;
}
extern "C" int test_overflow_new_epoch_no_history() {
  realtime::RealtimeBridge bridge;
  realtime::InputWorker worker(bridge);
  keyboard::ActionMapper mapper;
  app::ConfigCoordinator coordinator(bridge, mapper);
  Sink sink;
  uint32_t now = 0;
  worker.begin(0, now);
  auto c = defaults();
  c.keyboard.bindings[10] = {common::ActionType::Keyboard, 0x2c, 0, 0};
  CHECK(coordinator.start(c, now));
  settle(worker, coordinator, sink, 0, now);
  const auto old = coordinator.epoch();
  for (uint32_t i = 0; i < 128; ++i)
    CHECK(worker.submitPush(10, (i & 1) == 0, ++now));
  CHECK(!worker.submitPush(10, true, ++now));
  CHECK(worker.paused());
  CHECK(bridge.producer_fault_epoch.load() == old);
  CHECK(bridge.producer_overflows.load() == 1);
  CHECK(coordinator.start(c, now));
  settle(worker, coordinator, sink, 0, now);
  CHECK(coordinator.ready() && coordinator.epoch() != old && !worker.paused());
  CHECK(coordinator.stats().discarded_edges == 128);
  CHECK(mapper.processedMask() == (1u << 10));
  CHECK(sink.kc == 4 && sink.keys[3].held(0x2c));
  common::InputEdge edge;
  CHECK(!bridge.edges.tryPeek(edge));
  return 0;
}
extern "C" int test_timeout_stale_reply_retry() {
  realtime::RealtimeBridge bridge;
  realtime::InputWorker worker(bridge);
  keyboard::ActionMapper mapper;
  app::ConfigCoordinator coordinator(bridge, mapper);
  Sink sink;
  uint32_t now = 0;
  worker.begin(0, now);
  auto c = defaults();
  CHECK(coordinator.start(c, now));
  coordinator.service(1, sink);
  coordinator.service(2000001, sink);
  CHECK(coordinator.failed() && coordinator.stats().timeouts == 1 &&
        !coordinator.ready());
  now = 2000100;
  CHECK(coordinator.start(c, now));
  settle(worker, coordinator, sink, 0, now);
  CHECK(coordinator.ready());
  CHECK(coordinator.stats().stale_replies == 1);
  CHECK(coordinator.epoch() == 2);
  return 0;
}

// 用实际描述符的HID短项检查线缆格式，不读取注释或另写一套描述符。
struct HidLayout {
  uint16_t input_bits = 0, output_bits = 0;
  uint8_t inputs = 0, outputs = 0, depth = 0;
  bool has_report_id = false, relative_xy = false, buttons = false,
       absolute_xy = false, modifiers = false, bitmap = false;
};
bool parseHid(const uint8_t *bytes, uint16_t length, HidLayout &layout) {
  uint32_t size = 0, count = 0, page = 0, min = 0, max = 0;
  uint8_t usages[4]{};
  uint8_t usage_count = 0;
  for (uint16_t i = 0; i < length;) {
    const uint8_t prefix = bytes[i++];
    if (prefix == 0xfe)
      return false; // 本项目不使用long项。
    const uint8_t n = (prefix & 3) == 3 ? 4 : prefix & 3;
    if (i + n > length)
      return false;
    uint32_t value = 0;
    for (uint8_t j = 0; j < n; ++j)
      value |= uint32_t(bytes[i++]) << (8 * j);
    const uint8_t type = (prefix >> 2) & 3, tag = prefix >> 4;
    if (type == 1) {
      if (tag == 0)
        page = value;
      else if (tag == 7)
        size = value;
      else if (tag == 8)
        layout.has_report_id = true;
      else if (tag == 9)
        count = value;
    } else if (type == 2) {
      if (tag == 0 && usage_count < 4)
        usages[usage_count++] = static_cast<uint8_t>(value);
      else if (tag == 1)
        min = value;
      else if (tag == 2)
        max = value;
    } else if (type == 0) {
      if (tag == 10)
        ++layout.depth;
      else if (tag == 12) {
        if (!layout.depth)
          return false;
        --layout.depth;
      } else if (tag == 8 || tag == 9) {
        if (size * count > 512)
          return false;
        if (tag == 8) {
          layout.input_bits += size * count;
          ++layout.inputs;
          if (page == 7 && min == 0xe0 && max == 0xe7 && size == 1 &&
              count == 8 && value == 2)
            layout.modifiers = true;
          if (page == 7 && min == 0 && max == 0xdf && size == 1 &&
              count == 224 && value == 2)
            layout.bitmap = true;
          if (page == 9 && min == 1 && max == 3 && size == 1 && count == 3 &&
              value == 2)
            layout.buttons = true;
          if (page == 1 && size == 16 && count == 2 && value == 6 &&
              usage_count == 2 && usages[0] == 0x30 && usages[1] == 0x31)
            layout.relative_xy = true;
          if (page == 1 && size == 16 && count == 2 && value == 2 &&
              usage_count == 2 && usages[0] == 0x30 && usages[1] == 0x31)
            layout.absolute_xy = true;
        } else {
          layout.output_bits += size * count;
          ++layout.outputs;
        }
      }
      // local项只作用于下一条Main项。
      min = max = 0;
      usage_count = 0;
    }
  }
  return layout.depth == 0;
}
extern "C" int test_hid_descriptors_and_wire_encoding() {
  HidLayout keyboard_layout, mouse_layout;
  CHECK(parseHid(usb::kKeyboardDescriptor, usb::kKeyboardDescriptorBytes,
                 keyboard_layout));
  CHECK(keyboard_layout.input_bits == 29 * 8 &&
        keyboard_layout.output_bits == 8);
  CHECK(keyboard_layout.modifiers && keyboard_layout.bitmap &&
        !keyboard_layout.has_report_id);
  CHECK(parseHid(usb::kMouseDescriptor, usb::kMouseDescriptorBytes,
                 mouse_layout));
  CHECK(mouse_layout.input_bits == 10 * 8 && mouse_layout.output_bits == 0);
  CHECK(mouse_layout.buttons && mouse_layout.relative_xy && mouse_layout.absolute_xy &&
        mouse_layout.has_report_id);
  common::KeyboardState state;
  state.modifiers = 0xa5;
  // 按十二个不同usage，证明没有六键数组截断；最高支持usage也能保留。
  for (uint8_t i = 4; i < 16; ++i)
    state.keys[i >> 3] |= 1u << (i & 7);
  state.keys[0xa4 >> 3] |= 1u << (0xa4 & 7);
  // 外来保留/错误位不能作为键发送。
  state.keys[0] |= 0x0f;
  state.keys[27] = 0xff;
  uint8_t wire[usb::kKeyboardPayloadBytes];
  usb::encodeKeyboard(state, wire);
  CHECK(wire[0] == 0xa5 && (wire[1] & 15) == 0);
  for (uint8_t i = 4; i < 16; ++i)
    CHECK(wire[1 + (i >> 3)] & (1u << (i & 7)));
  CHECK(wire[1 + (0xa4 >> 3)] == 0x10 && wire[28] == 0);
  uint8_t mouse[usb::kMousePayloadBytes];
  usb::encodeRelativeMouse(0xff, -32768, 32767, mouse);
  CHECK(mouse[0] == 7 && mouse[1] == 0 && mouse[2] == 0x80 &&
        mouse[3] == 0xff && mouse[4] == 0x7f);
  usb::encodeRelativeMouse(5, -1, -256, mouse);
  CHECK(mouse[0] == 5 && mouse[1] == 0xff && mouse[2] == 0xff &&
        mouse[3] == 0 && mouse[4] == 0xff);
  return 0;
}
extern "C" int test_replace_busy_apply_keeps_confirmed_config() {
  realtime::RealtimeBridge bridge;
  realtime::InputWorker worker(bridge);
  keyboard::ActionMapper mapper;
  app::ConfigCoordinator coordinator(bridge, mapper);
  Sink sink;
  uint32_t now = 0;
  worker.begin(0, now);
  auto c = defaults();
  c.revision = 2;
  CHECK(coordinator.start(c, now));
  settle(worker, coordinator, sink, 0, now);
  CHECK(coordinator.hasApplied() && coordinator.ready() &&
        coordinator.appliedConfig().revision == 2);
  auto next = c;
  next.revision = 3;
  next.keyboard.bindings[0].usage = 0x2c;
  sink.blocked = true;
  CHECK(coordinator.start(next, now));
  for (uint8_t i = 0; i < 12; ++i)
    tick(worker, coordinator, sink, 1, now);
  CHECK(coordinator.busy() && !coordinator.ready() && worker.paused());
  CHECK(coordinator.appliedConfig().revision ==
        2); // 指示灯必须仍采用最后确认配置。
  CHECK(!coordinator.start(c, now));      // 正常请求不可抢占。
  CHECK(coordinator.start(c, now, true)); // USB故障恢复可以新epoch替换。
  sink.blocked = false;
  settle(worker, coordinator, sink, 1, now);
  CHECK(coordinator.ready() && coordinator.epoch() == 3 &&
        coordinator.appliedConfig().revision == 2);
  CHECK(mapper.processedMask() == 1 && sink.keys[sink.kc - 1].held(0x33));
  CHECK(!sink.keys[sink.kc - 1].held(0x2c));
  // 旧Resume已排队、尚未ACK时替换，同样不能让旧ACK宣布新申请完成。
  next.revision = 4;
  CHECK(coordinator.start(next, now));
  coordinator.service(now, sink); // QueuePause -> WaitPause。
  worker.service(1, ++now);
  worker.service(1, ++now);
  coordinator.service(now, sink);
  for (uint8_t i = 0; i < 5; ++i)
    coordinator.service(++now, sink); // 四报告后Resume排队。
  CHECK(coordinator.busy());
  CHECK(coordinator.start(c, now, true));
  settle(worker, coordinator, sink, 1, now);
  CHECK(coordinator.ready() && coordinator.epoch() == 5 &&
        coordinator.appliedConfig().revision == 2);
  CHECK(coordinator.stats().stale_replies >= 1);
  return 0;
}

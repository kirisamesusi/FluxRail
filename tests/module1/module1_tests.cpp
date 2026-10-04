#include "keyboard/ActionMapper.h"
#include "keyboard/KeyScanner.h"
#include "lighting/KeyLighting.h"
#include <stddef.h>
using namespace fluxrail::keyboard;
using namespace fluxrail::lighting;

// freestanding ARM测试无需Arduino/USB/硬件；真实模块.cpp原样链接并执行。
extern "C" void *memset(void *dst, int value, size_t count) {
    auto bytes = static_cast<unsigned char *>(dst);
    for (size_t i = 0; i < count; ++i)
        bytes[i] = static_cast<unsigned char>(value);
    return dst;
}
extern "C" void *memcpy(void *dst, const void *src, size_t count) {
    auto to = static_cast<unsigned char *>(dst);
    auto from = static_cast<const unsigned char *>(src);
    for (size_t i = 0; i < count; ++i)
        to[i] = from[i];
    return dst;
}
void operator delete(void *) noexcept {}
void operator delete(void *, size_t) noexcept {}
extern "C" void __cxa_pure_virtual() {
    for (;;) {
    }
}
volatile uint32_t checks = 0;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition))                                                                          \
            return __LINE__;                                                                       \
    } while (0)

struct ReportLog : ActionSink {
    KeyboardState keyboard[256]{};
    uint8_t mouse[256]{};
    uint32_t keyboard_count = 0, mouse_count = 0;
    bool busy = false;
    bool tryKeyboard(const KeyboardState &state) override {
        if (busy || keyboard_count == 256)
            return false;
        keyboard[keyboard_count++] = state;
        return true;
    }
    bool tryMouseButtons(uint8_t state) override {
        if (busy || mouse_count == 256)
            return false;
        mouse[mouse_count++] = state;
        return true;
    }
    void clear() { keyboard_count = mouse_count = 0; }
};
void settleReset(ActionMapper &mapper, ReportLog &sink) {
    for (uint32_t i = 0; i < 4; ++i)
        mapper.service(i * 1000, sink);
    sink.clear();
}
void drain(ActionMapper &mapper, ReportLog &sink, uint32_t start = 10000) {
    for (uint32_t i = 0; i < 200 && !mapper.idle(); ++i)
        mapper.service(start + i * 1000, sink);
}
Binding key(uint8_t usage, uint8_t modifiers = 0) {
    return {ActionType::Keyboard, usage, modifiers, 0};
}
Binding mouse(uint8_t button) { return {ActionType::MouseButton, 0, 0, button}; }

extern "C" int test_bindings_ascii() {
    const auto c = defaultKeyboardConfig();
    const uint8_t expected[] = {0x33, 0x0f, 0x0e, 0x0d, 9, 7, 0x16, 4, 0x19, 0x11};
    CHECK(validConfig(c));
    for (uint8_t i = 0; i < 10; ++i)
        CHECK(c.bindings[i].usage == expected[i]);
    CHECK(c.bindings[10].type == ActionType::Disabled &&
          c.bindings[11].type == ActionType::Disabled);
    Binding b;
    for (char ch = 'a'; ch <= 'z'; ++ch) {
        CHECK(bindingFromUsAscii(ch, b));
        CHECK(b.usage == 4 + ch - 'a' && b.modifiers == 0);
    }
    for (char ch = 'A'; ch <= 'Z'; ++ch) {
        CHECK(bindingFromUsAscii(ch, b));
        CHECK(b.usage == 4 + ch - 'A' && b.modifiers == 2);
    }
    const char normal[] = {' ', '-', '=', '[', ']', 0x5c, ';', 0x27, ',', '.', '/', 0x60};
    const char shifted[] = {0, '_', '+', '{', '}', '|', ':', 0x22, '<', '>', '?', '~'};
    const uint8_t usages[] = {0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31,
                              0x33, 0x34, 0x36, 0x37, 0x38, 0x35};
    for (uint8_t i = 0; i < 12; ++i) {
        CHECK(bindingFromUsAscii(normal[i], b));
        CHECK(b.usage == usages[i] && !b.modifiers);
        if (i) {
            CHECK(bindingFromUsAscii(shifted[i], b));
            CHECK(b.usage == usages[i] && b.modifiers == 2);
        }
    }
    const char symbols[] = "!@#$%^&*()";
    for (uint8_t i = 0; i < 10; ++i) {
        CHECK(bindingFromUsAscii(symbols[i], b));
        CHECK(b.usage == 0x1e + i && b.modifiers == 2);
    }
    const Binding previous = b;
    CHECK(!bindingFromUsAscii('\n', b));
    CHECK(b.usage == previous.usage);
    CHECK(!validBinding(key(1)));
    CHECK(!validBinding(mouse(3)));
    CHECK(!validBinding(key(0)));
    for (uint8_t i = 0xe0; i <= 0xe7; ++i)
        CHECK(validBinding(key(i)));
    return 0;
}
extern "C" int test_twelve_nkro() {
    KeyboardConfig c;
    for (uint8_t id = 0; id < 12; ++id)
        c.bindings[id] = key(4 + id);
    ActionMapper mapper;
    ReportLog sink;
    CHECK(mapper.reset(c, 1, 0));
    settleReset(mapper, sink);
    for (uint8_t id = 0; id < 12; ++id)
        CHECK(mapper.enqueue(id, true, id, 100, 1) == IngressResult::Accepted);
    drain(mapper, sink);
    CHECK(mapper.idle());
    CHECK(mapper.processedMask() == 0x0fff);
    CHECK(sink.keyboard_count == 12);
    for (uint8_t id = 0; id < 12; ++id)
        CHECK(sink.keyboard[11].held(4 + id));
    for (uint8_t id = 0; id < 12; ++id)
        CHECK(mapper.enqueue(id, false, 12 + id, 200, 1) == IngressResult::Accepted);
    drain(mapper, sink);
    CHECK(mapper.processedMask() == 0);
    CHECK(sink.keyboard_count == 24);
    for (uint8_t id = 0; id < 12; ++id)
        CHECK(!sink.keyboard[23].held(4 + id));
    return 0;
}
extern "C" int test_duplicate_busy_overlap() {
    // 同样的动作表对字母、数字、Space、方向键和F键都成立，不能特判A。
    const uint8_t usages[] = {4, 0x1e, 0x2c, 0x50, 0x3a};
    for (auto usage : usages) {
        KeyboardConfig c;
        c.bindings[0] = key(usage);
        c.bindings[1] = key(usage);
        c.bindings[2] = key(usage);
        c.bindings[3] = key(5, 2);
        ActionMapper m;
        ReportLog s;
        CHECK(m.reset(c, 7, 0));
        settleReset(m, s);
        CHECK(m.enqueue(3, true, 0, 0, 7) == IngressResult::Accepted);
        m.service(0, s);
        CHECK(m.enqueue(0, true, 1, 0, 7) == IngressResult::Accepted);
        m.service(1000, s);
        CHECK(m.enqueue(1, true, 2, 0, 7) == IngressResult::Accepted);
        CHECK(m.enqueue(1, false, 3, 0, 7) == IngressResult::Accepted); // 极短按也在对应DOWN之后。
        CHECK(m.enqueue(2, true, 4, 0, 7) == IngressResult::Accepted);
        s.busy = true;
        m.service(2000, s);
        m.service(3000, s);
        CHECK(s.keyboard_count == 2);
        s.busy = false;
        m.service(4000, s);
        CHECK(s.keyboard_count == 3);
        CHECK(!s.keyboard[2].held(usage) && s.keyboard[2].held(5) && s.keyboard[2].modifiers == 2);
        m.service(4999, s);
        CHECK(s.keyboard_count == 3);
        m.service(5000, s);
        CHECK(s.keyboard_count == 4);
        CHECK(s.keyboard[3].held(usage));
        drain(m, s, 6000);
        CHECK(s.keyboard_count == 6 && m.stats().retriggers == 2);
        CHECK(!s.keyboard[4].held(usage) && s.keyboard[5].held(usage));
        CHECK(m.enqueue(0, false, 5, 0, 7) == IngressResult::Accepted);
        drain(m, s);
        CHECK(s.keyboard_count == 6);
        CHECK(m.enqueue(2, false, 6, 0, 7) == IngressResult::Accepted);
        drain(m, s);
        CHECK(s.keyboard_count == 7);
        CHECK(!s.keyboard[6].held(usage) && s.keyboard[6].held(5) && s.keyboard[6].modifiers == 2);
    }
    return 0;
}
extern "C" int test_modifiers_and_mouse() {
    KeyboardConfig c;
    c.bindings[0] = key(0xe1);
    c.bindings[1] = key(0xe1);
    c.bindings[2] = key(4, 2);
    c.bindings[3] = mouse(1);
    c.bindings[4] = mouse(1);
    c.bindings[5] = mouse(2);
    c.bindings[6] = mouse(4);
    ActionMapper m;
    ReportLog s;
    CHECK(m.reset(c, 1, 0));
    settleReset(m, s);
    CHECK(m.enqueue(0, true, 0, 0, 1) == IngressResult::Accepted);
    m.service(1000, s);
    CHECK(m.enqueue(1, true, 1, 0, 1) == IngressResult::Accepted);
    drain(m, s);
    CHECK(s.keyboard_count == 3);
    CHECK(s.keyboard[0].modifiers == 2);
    CHECK(s.keyboard[1].modifiers == 0 && s.keyboard[2].modifiers == 2);
    CHECK(m.enqueue(2, true, 2, 0, 1) == IngressResult::Accepted);
    drain(m, s);
    for (uint8_t id = 3; id < 7; ++id)
        CHECK(m.enqueue(id, true, id, 0, 1) == IngressResult::Accepted);
    drain(m, s);
    CHECK(s.mouse_count == 3);
    CHECK(s.mouse[2] == 7);
    CHECK(m.enqueue(3, false, 7, 0, 1) == IngressResult::Accepted);
    drain(m, s);
    CHECK(s.mouse_count == 3);
    CHECK(m.enqueue(4, false, 8, 0, 1) == IngressResult::Accepted);
    drain(m, s);
    CHECK(s.mouse[3] == 6);
    CHECK(s.keyboard[3].held(4) && s.keyboard[3].modifiers == 2);
    // A已按住时再按Shift+A，目标UP须保留旧修饰状态，不能提前添加Shift。
    KeyboardConfig combos;
    combos.bindings[0] = key(4);
    combos.bindings[1] = key(4, 2);
    CHECK(m.reset(combos, 2, 0));
    settleReset(m, s);
    CHECK(m.enqueue(0, true, 0, 0, 2) == IngressResult::Accepted);
    m.service(0, s);
    CHECK(m.enqueue(1, true, 1, 1000, 2) == IngressResult::Accepted);
    m.service(1000, s);
    CHECK(!s.keyboard[1].held(4) && s.keyboard[1].modifiers == 0);
    m.service(2000, s);
    CHECK(s.keyboard[2].held(4) && s.keyboard[2].modifiers == 2);
    CHECK(m.enqueue(1, false, 2, 3000, 2) == IngressResult::Accepted);
    m.service(3000, s);
    CHECK(s.keyboard[3].held(4) && s.keyboard[3].modifiers == 0);
    return 0;
}
extern "C" int test_backpressure_revision_recovery() {
    ActionMapper m;
    ReportLog s;
    auto c = defaultKeyboardConfig();
    CHECK(m.reset(c, 10, 0));
    settleReset(m, s);
    CHECK(m.enqueue(0, true, 0xfffffffe, 0, 10) == IngressResult::Accepted);
    CHECK(m.enqueue(0, false, 0xffffffff, 0, 10) == IngressResult::Accepted);
    CHECK(m.enqueue(0, true, 0, 0, 10) == IngressResult::Accepted); // 序号回绕。
    CHECK(m.enqueue(0, false, 1, 0, 9) == IngressResult::StaleRevision);
    for (uint32_t seq = 1; seq <= 61; ++seq)
        CHECK(m.enqueue(0, (seq & 1) == 0, 0 + seq, 0, 10) == IngressResult::Accepted);
    CHECK(m.enqueue(0, true, 62, 0, 10) == IngressResult::Backpressure); // 拒绝不推进sequence。
    m.service(1000, s);
    CHECK(m.enqueue(0, true, 62, 0, 10) == IngressResult::Accepted);
    CHECK(m.stats().queue_high_water == 64);
    drain(m, s);
    CHECK(m.idle());
    CHECK(m.enqueue(0, true, 64, 0, 10) == IngressResult::NeedsResync);
    CHECK(m.needsResync());
    const auto reports = s.keyboard_count;
    m.service(50000, s);
    CHECK(s.keyboard_count == reports);
    CHECK(m.reset(c, 11, 1u << 7));
    s.clear();
    s.busy = true;
    m.service(0, s);
    CHECK(m.recovering());
    s.busy = false;
    for (uint8_t i = 0; i < 4; ++i)
        m.service(i * 1000, s);
    CHECK(!m.recovering());
    CHECK(s.keyboard_count == 2 && s.mouse_count == 2);
    CHECK(!s.keyboard[0].held(4) && s.keyboard[1].held(4));
    CHECK(m.enqueue(7, false, 1, 0, 10) == IngressResult::StaleRevision);
    auto changed = c;
    changed.bindings[7] = key(0x2c);
    CHECK(m.reset(changed, 12, 1u << 7));
    s.clear();
    for (uint8_t i = 0; i < 4; ++i)
        m.service(i * 1000, s);
    CHECK(!s.keyboard[0].held(4) && !s.keyboard[1].held(4) && s.keyboard[1].held(0x2c));
    auto invalid = changed;
    invalid.bindings[7].usage = 1;
    CHECK(!m.reset(invalid, 13, 0));
    CHECK(m.revision() == 12);
    return 0;
}
extern "C" int test_retrigger_time_wrap() {
    KeyboardConfig c;
    c.bindings[0] = key(4);
    c.bindings[1] = key(4);
    ActionMapper m;
    ReportLog s;
    CHECK(m.reset(c, 1, 0));
    settleReset(m, s);
    CHECK(m.enqueue(0, true, 0, 0, 1) == IngressResult::Accepted);
    m.service(0xfffff000, s);
    CHECK(m.enqueue(1, true, 1, 0, 1) == IngressResult::Accepted);
    m.service(0xffffff00, s);
    CHECK(s.keyboard_count == 2 && !s.keyboard[1].held(4));
    m.service(0x100, s);
    CHECK(s.keyboard_count == 2);
    m.service(0x2e8, s);
    CHECK(s.keyboard_count == 3);
    CHECK(s.keyboard[2].held(4));
    return 0;
}
struct EdgeLog : EdgeSink {
    uint8_t ids[32]{};
    bool states[32]{};
    uint32_t timestamps[32]{};
    uint32_t revisions[32]{};
    uint8_t count = 0;
    bool reject = false;
    bool tryEdge(uint8_t id, bool down, uint32_t time, uint32_t rev) override {
        if (reject || count == 32)
            return false;
        ids[count] = id;
        states[count] = down;
        timestamps[count] = time;
        revisions[count] = rev;
        ++count;
        return true;
    }
};
extern "C" int test_scanner_bounce_overflow_wrap() {
    KeyScanner scanner;
    ScanConfig c;
    EdgeLog log;
    CHECK(scanner.reset(c, 0, 0, 4));
    scanner.sample(1, 100, log);
    scanner.sample(0, 500, log);
    scanner.sample(1, 600, log);
    scanner.sample(1, 1599, log);
    CHECK(log.count == 0);
    scanner.sample(1, 1600, log);
    CHECK(log.count == 1 && log.ids[0] == 0 && log.states[0]);
    CHECK(log.timestamps[0] == 600 && log.revisions[0] == 4);
    scanner.sample(0, 2000, log);
    scanner.sample(1, 2200, log);
    scanner.sample(0, 2400, log);
    scanner.sample(0, 3400, log);
    CHECK(log.count == 2 && !log.states[1]);
    log.reject = true;
    scanner.sample(3, 4000, log);
    scanner.sample(3, 5000, log);
    CHECK(scanner.needsResync() && scanner.stats().overflow_faults == 1 &&
          scanner.physicalMask() == 3);
    log.reject = false;
    scanner.sample(0, 6000, log);
    scanner.sample(0, 7000, log);
    CHECK(log.count == 2 && scanner.physicalMask() == 0); // 故障期间继续真实扫描，不偷发历史。
    CHECK(scanner.reset(c, 1, 0xffffff00, 5));
    scanner.sample(0, 0xffffff10, log);
    scanner.sample(0, 0x2f8, log);
    CHECK(log.count == 3 && !log.states[2] && log.revisions[2] == 5);
    CHECK(scanner.stats().max_edge_delay_us >= 1000);
    c.press_debounce_us = 20001;
    CHECK(!scanner.reset(c, 0, 0, 6));
    return 0;
}
extern "C" int test_lighting_modes_bounds() {
    LightingConfig c;
    LedOrder order;
    CHECK(validLightingConfig(c) && validLedOrder(order));
    for (uint8_t mode = 0; mode < 5; ++mode)
        for (uint8_t level = 0; level <= 100; ++level)
            for (uint8_t step = 0; step < 8; ++step) {
                c.mode = static_cast<LightMode>(mode);
                c.brightness_level = level;
                const auto frame = renderLights(c, 0x03ff, step * 251, true, {255, 255, 255});
                for (const auto &rgb : frame.keys)
                    CHECK(rgb.r <= 127 && rgb.g <= 127 && rgb.b <= 127);
                CHECK(frame.indicator.r == 63 && frame.indicator.g == 63 &&
                      frame.indicator.b == 63);
                if (level == 0)
                    for (const auto &rgb : frame.keys)
                        CHECK(!rgb.r && !rgb.g && !rgb.b);
            }
    c.mode = LightMode::Static;
    c.brightness_level = 100;
    CHECK(renderLights(c, 0, 0, false, {}).keys[0].r == 127);
    c.brightness_level = 50;
    CHECK(renderLights(c, 0, 0, false, {}).keys[0].r == 63);
    c.brightness_level = 101;
    const auto invalid = renderLights(c, 0x03ff, 0, false, {});
    CHECK(!invalid.keys[0].r);
    c.brightness_level = 100;
    c.mode = LightMode::Press;
    c.per_key = true;
    c.key_colors[7] = {200, 100, 50};
    const auto pressed = renderLights(c, 1u << 7, 0, true, {255, 0, 0});
    CHECK(pressed.keys[7].r == 100 && pressed.keys[7].g == 50 && pressed.keys[7].b == 25);
    CHECK(!pressed.keys[0].r && pressed.indicator.r == 63);
    c.mode = LightMode::RainbowWave;
    renderLights(c, 0, 500, false, {});
    CHECK(c.key_colors[7].r == 200);
    c.mode = LightMode::Press;
    CHECK(renderLights(c, 1u << 7, 0, false, {}).keys[7].r == 100);
    c.enabled = false;
    CHECK(!renderLights(c, 0x03ff, 0, true, {255, 0, 0}).keys[7].r);
    return 0;
}
extern "C" int test_final_output_cap_and_order() {
    LightFrame frame;
    LedOrder order;
    for (auto &c : frame.keys)
        c = {255, 255, 255};
    frame.indicator = {255, 255, 255};
    uint32_t words[10], indicator;
    packLightWords(frame, order, true, words, indicator);
    for (auto word : words)
        CHECK(word == 0x7f7f7f00);
    CHECK(indicator == 0x3f3f3f00);
    for (uint8_t id = 0; id < 10; ++id)
        frame.keys[id] = {id, static_cast<uint8_t>(id + 20), static_cast<uint8_t>(id + 40)};
    packLightWords(frame, order, true, words, indicator);
    CHECK(words[0] == 0x1c083000 && words[8] == 0x14002800);
    packLightWords(frame, order, false, words, indicator);
    CHECK(words[0] == 0x081c3000);
    order.chain_to_bt[0] = order.chain_to_bt[1];
    CHECK(!validLedOrder(order));
    packLightWords(frame, order, true, words, indicator);
    for (auto word : words)
        CHECK(word == 0);
    return 0;
}

// 覆盖每个支持usage（含八个独立修饰），十二物理来源同时持有。
// 相同目标的每次新DOWN都需独立UP/DOWN，其他键/修饰保持。
extern "C" int test_all_supported_usages_twelve_sources_retrigger() {
    for (uint16_t value = 4; value <= 0xe7; ++value) {
        if (value > 0xa4 && value < 0xe0)
            continue;
        const auto usage = static_cast<uint8_t>(value);
        KeyboardConfig c;
        const uint8_t other = usage == 4 ? 5 : 4;
        const uint8_t modifier = usage == 0xe1 ? 4 : 2;
        for (uint8_t id = 0; id < 11; ++id)
            c.bindings[id] = key(usage);
        c.bindings[11] = key(other, modifier);
        ActionMapper m;
        ReportLog s;
        CHECK(m.reset(c, 1, 0));
        settleReset(m, s);
        CHECK(m.enqueue(11, true, 0, 0, 1) == IngressResult::Accepted);
        m.service(1000, s);
        for (uint8_t id = 0; id < 11; ++id)
            CHECK(m.enqueue(id, true, id + 1, 0, 1) == IngressResult::Accepted);
        drain(m, s);
        CHECK(m.idle() && m.processedMask() == 0xfff &&
              s.keyboard_count == 22 && m.stats().retriggers == 10);
        CHECK(s.keyboard[1].held(usage));
        for (uint8_t i = 2; i < 22; i += 2) {
            CHECK(!s.keyboard[i].held(usage) && s.keyboard[i + 1].held(usage));
            CHECK(s.keyboard[i].held(other) && s.keyboard[i + 1].held(other));
            CHECK((s.keyboard[i].modifiers & modifier) == modifier);
        }
        for (uint8_t id = 0; id < 10; ++id)
            CHECK(m.enqueue(id, false, 12 + id, 0, 1) == IngressResult::Accepted);
        drain(m, s);
        CHECK(s.keyboard_count == 22 && m.processedMask() == 0xc00);
        CHECK(m.enqueue(10, false, 22, 0, 1) == IngressResult::Accepted);
        drain(m, s);
        CHECK(s.keyboard_count == 23 && !s.keyboard[22].held(usage) &&
              s.keyboard[22].held(other));
        CHECK(m.enqueue(11, false, 23, 0, 1) == IngressResult::Accepted);
        drain(m, s);
        CHECK(m.processedMask() == 0 && s.keyboard_count == 24 &&
              s.keyboard[23].modifiers == 0 && !s.keyboard[23].held(other));
    }
    return 0;
}
extern "C" int test_shared_key_brightness_and_indicator_policy() {
    const uint8_t envelopes[] = {0, 1, 128, 255};
    for (uint16_t value = 0; value <= 255; value += 17)
        for (auto envelope : envelopes)
            for (uint8_t level = 0; level <= 100; ++level) {
                const auto channel = static_cast<uint8_t>(value);
                const auto out = scaleKeyColor({channel, channel, channel}, envelope, level);
                const auto expected = value * envelope * level / (255u * 200u);
                CHECK(out.r == expected && out.g == expected && out.b == expected);
                CHECK(out.r <= 127);
            }
    const auto invalid = scaleKeyColor({255, 255, 255}, 255, 101);
    CHECK(!invalid.r && !invalid.g && !invalid.b);
    const auto normal = modeIndicatorColor({17, 33, 129}, false);
    const auto io4 = modeIndicatorColor({17, 33, 129}, true);
    CHECK(normal.r == 17 && normal.g == 33 && normal.b == 129);
    CHECK(io4.r == 255 && !io4.g && io4.b == 255);
    const auto disabled = renderLights({}, 0, 0, false, io4).indicator;
    CHECK(!disabled.r && !disabled.g && !disabled.b);
    return 0;
}

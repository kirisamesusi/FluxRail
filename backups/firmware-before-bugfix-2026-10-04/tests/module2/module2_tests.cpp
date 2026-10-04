#include "hall/HallModule.h"
#include "hall/HallSampler.h"
#include <stddef.h>
using namespace fluxrail::hall;

// 原始模块.cpp以ARM指令执行。这里只生成合成输入和最小freestanding运行时。
extern "C" void *memset(void *dst, int value, size_t count) {
  auto p = static_cast<unsigned char *>(dst);
  for (size_t i = 0; i < count; ++i)
    p[i] = static_cast<unsigned char>(value);
  return dst;
}
extern "C" void *memcpy(void *dst, const void *src, size_t count) {
  auto p = static_cast<unsigned char *>(dst);
  auto q = static_cast<const unsigned char *>(src);
  for (size_t i = 0; i < count; ++i) {
    p[i] = q[i];
  }
  return dst;
}
void operator delete(void *) noexcept {}
void operator delete(void *, size_t) noexcept {}
extern "C" void __cxa_pure_virtual() {
  for (;;) {
  }
}
volatile uint32_t checks = 0;
extern "C" const uint32_t hall_layout[6] = {
    sizeof(Sample),     sizeof(Estimate),     sizeof(Calibration),
    sizeof(HallModule), sizeof(HallPosition), sizeof(HallCalibration)};
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x))                                                                  \
      return __LINE__;                                                         \
  } while (0)
static bool near(float a, float b, float eps = 0.08f) {
  return magnitude(a - b) < eps;
}
static Calibration fixture() {
  Calibration c{};
  c.valid = true;
  c.left_mm = -41;
  c.right_mm = 39;
  c.node_count = 7;
  const float positions[7] = {-41, kSensorMm[0],        kSensorMm[0] * 0.5f,
                              0,   kSensorMm[2] * 0.5f, kSensorMm[2],
                              39};
  c.polarity[1] = -1;
  for (uint8_t i = 0; i < 3; ++i) {
    c.baseline[i] = 2048;
    c.baseline_noise[i] = 0.3f;
  }
  for (uint8_t j = 0; j < 7; ++j) {
    c.nodes[j].mm = positions[j];
    for (uint8_t i = 0; i < 3; ++i) {
      c.nodes[j].response[i] =
          100 + 900 * clamp(1 - magnitude(positions[j] - kSensorMm[i]) / 33.75f,
                            0, 1);
      c.nodes[j].noise[i] = 0.5f;
    }
  }
  c.noise_mm = 0.02f;
  c.sweep_left = true;
  c.sweep_right = true;
  c.sweep_segments = 63;
  return c;
}
static Sample at(float mm, const Calibration &c, uint32_t seq, uint32_t time) {
  Sample s{};
  s.sequence = seq;
  s.timestamp_us = time;
  uint8_t j = 0;
  while (j + 2 < c.node_count && mm > c.nodes[j + 1].mm)
    ++j;
  const Node &a = c.nodes[j], &b = c.nodes[j + 1];
  const float t = clamp((mm - a.mm) / (b.mm - a.mm), 0, 1);
  for (uint8_t i = 0; i < 3; ++i) {
    const float r = a.response[i] + t * (b.response[i] - a.response[i]);
    s.raw[i] = static_cast<uint16_t>(c.baseline[i] +
                                     r / (c.polarity[i] * c.gain[i]) + 0.5f);
  }
  return s;
}
static Settings enabled() {
  Settings s{};
  s.enabled = true;
  s.quiet_tau_us = 0;
  return s;
}

extern "C" int test_absolute_return_endpoints_gain_invert() {
  auto c = fixture();
  CHECK(validateCalibration(c));
  HallPosition p;
  auto settings = enabled();
  CHECK(p.apply(settings, 13, 21));
  CHECK(p.load(c));
  uint32_t seq = 0, time = 0;
  for (int pass = 0; pass < 3; ++pass)
    for (int i = 0; i <= 320; ++i) {
      const float mm = pass % 2 ? 39 - i * 0.25f : -41 + i * 0.25f;
      auto e = p.process(at(mm, c, seq++, time += 1000));
      CHECK(e.valid);
      CHECK(near(e.mm, mm));
      CHECK(near(e.p, (mm + 41) / 80, 0.001f));
      CHECK(e.revision == 13 && e.epoch == 21);
    }
  auto left = p.process(at(-41, c, seq++, time += 1000));
  CHECK(near(left.mm, -41));
  CHECK(near(left.p, 0));
  auto right = p.process(at(39, c, seq++, time += 1000));
  CHECK(near(right.mm, 39));
  CHECK(near(right.p, 1));
  // centroid被传感器中心限制，最终标定mm仍能覆盖外侧实测端点。
  CHECK(left.mm < kSensorMm[0]);
  CHECK(right.mm > kSensorMm[2]);
  settings.invert = true;
  settings.center_gain = 2;
  settings.range_min = 0.2f;
  settings.range_max = 0.8f;
  CHECK(p.apply(settings, 14, 22));
  CHECK(near(p.process(at(-41, c, seq++, time += 1000)).output, 0.8f, 0.001f));
  CHECK(near(p.process(at(39, c, seq++, time += 1000)).output, 0.2f, 0.001f));
  settings.center_gain = 0.5f;
  CHECK(p.apply(settings, 15, 23));
  CHECK(near(p.process(at(-41, c, seq++, time += 1000)).output, 0.65f, 0.001f));
  return 0;
}
extern "C" int test_failure_reentry_and_parameter_atomicity() {
  auto c = fixture();
  HallPosition p;
  auto settings = enabled();
  CHECK(p.apply(settings, 3, 4));
  CHECK(p.load(c));
  CHECK(p.process(at(12, c, 0, 1000)).valid);
  Sample absent{};
  for (auto &r : absent.raw)
    r = 2048;
  absent.timestamp_us = 2000;
  auto e = p.process(absent);
  CHECK(!e.valid && e.quality == Quality::WeakField);
  CHECK(near(e.mm, 12));
  auto saturation = at(10, c, 2, 3000);
  saturation.raw[1] = 4095;
  CHECK(p.process(saturation).quality == Quality::Saturated);
  auto hardware = at(10, c, 3, 4000);
  hardware.ok = false;
  CHECK(p.process(hardware).quality == Quality::SamplingError);
  auto off = at(10, c, 4, 5000);
  off.raw[0] = 4000;
  CHECK(p.process(off).quality == Quality::OffCurve);
  e = p.process(at(-30, c, 5, 6000));
  CHECK(e.valid && near(e.mm, -30) && !e.active);
  auto bad = settings;
  bad.center_gain = 0;
  CHECK(!p.apply(bad, 999, 999));
  CHECK(p.process(at(-30, c, 6, 7000)).revision == 3);
  bad = settings;
  bad.range_max = bad.range_min;
  CHECK(!validateSettings(bad));
  bad = settings;
  bad.fast_speed_mm_s = __builtin_nanf("");
  CHECK(!validateSettings(bad));
  auto corrupted = c;
  corrupted.nodes[2].response[0] = __builtin_nanf("");
  CHECK(!p.load(corrupted));
  CHECK(p.process(at(25, c, 7, 8000)).valid);
  settings.enabled = false;
  CHECK(p.apply(settings, 4, 5));
  e = p.process(at(25, c, 8, 9000));
  CHECK(!e.valid && !e.active && e.quality == Quality::Disabled);
  return 0;
}
extern "C" int test_activity_slow_motion_filter_dt_wrap_reverse() {
  auto c = fixture();
  auto s = enabled();
  s.quiet_tau_us = 1500;
  HallPosition p;
  CHECK(p.load(c));
  CHECK(p.apply(s, 1, 1));
  uint32_t time = 0xfffff000u, seq = 0;
  CHECK(!p.process(at(0, c, seq++, time)).active);
  bool slow_active = false;
  for (int i = 1; i <= 100; ++i) {
    auto e = p.process(at(i * 0.01f, c, seq++, time += 1000));
    CHECK(e.valid);
    if (e.active)
      slow_active = true;
  }
  CHECK(slow_active);
  auto moved = p.process(at(30, c, seq++, time += 1000));
  CHECK(near(moved.mm, 30));
  auto reverse = p.process(at(-30, c, seq++, time += 1000));
  CHECK(near(reverse.mm, -30));
  for (int i = 0; i < 30; ++i)
    CHECK(!p.process(at(-30, c, seq++, time += 1000)).active || i < 10);
  auto reentry = p.process(at(25, c, seq++, time += 20000));
  CHECK(near(reentry.mm, 25) && !reentry.active);
  // 相同输入但gain不同，活动门槛应在mm域完全一致。
  HallPosition a, b;
  s.center_gain = 0.5f;
  CHECK(a.load(c));
  CHECK(a.apply(s, 1, 1));
  s.center_gain = 4;
  CHECK(b.load(c));
  CHECK(b.apply(s, 1, 1));
  for (int i = 0; i < 200; ++i) {
    auto input = at(-10 + i * 0.04f, c, i, i * 1000);
    CHECK(a.process(input).active == b.process(input).active);
  }
  return 0;
}
extern "C" int test_saved_calibration_rejection_ambiguity() {
  auto c = fixture();
  CHECK(validateCalibration(c));
  auto bad = c;
  bad.version = 99;
  CHECK(!validateCalibration(bad));
  bad = c;
  bad.sweep_segments = 31;
  CHECK(!validateCalibration(bad));
  bad = c;
  bad.nodes[1].mm = bad.nodes[0].mm;
  CHECK(!validateCalibration(bad));
  bad = c;
  bad.node_count = 18;
  CHECK(!validateCalibration(bad));
  bad = c;
  bad.node_count = 0;
  CHECK(!validateCalibration(bad));
  bad = c;
  bad.left_mm = -30;
  CHECK(!validateCalibration(bad));
  bad = c;
  bad.nodes[2].noise[0] = 81;
  CHECK(!validateCalibration(bad));
  bad = c;
  bad.gain[0] = 0;
  CHECK(!validateCalibration(bad));
  bad = c;
  bad.polarity[2] = 0;
  CHECK(!validateCalibration(bad));
  bad = c;
  for (uint8_t i = 0; i < 3; ++i)
    bad.nodes[5].response[i] = bad.nodes[1].response[i];
  CHECK(!validateCalibration(bad));
  CHECK(project(bad.nodes[1].response, bad).ambiguous);
  // 开机复用直接load得到有效当前位置，不需要空场/重新扫描。
  HallPosition p;
  CHECK(p.load(c));
  CHECK(p.apply(enabled(), 8, 9));
  CHECK(p.process(at(20, c, 1, 1000)).valid);
  return 0;
}
struct CalibrationFeed {
  uint32_t seq = 0, time = 0;
  template <class Job> void capture(Job &job, Sample sample, uint16_t count) {
    for (uint16_t i = 0; i < count; ++i) {
      sample.sequence = seq++;
      sample.timestamp_us = time += 1000;
      job.sample(sample);
    }
  }
};
static bool startJob(HallCalibration &job, uint32_t time,
                     const Calibration &c) {
  return job.start(1, time, true, c.left_mm, c.right_mm);
}
static bool startJob(HallModule &module, uint32_t time, const Calibration &c) {
  return module.startCalibration(1, time, true, c.left_mm, c.right_mm);
}
template <class Job>
static bool prepare(Job &job, CalibrationFeed &feed, const Calibration &c,
                    bool check_map = true) {
  if (!startJob(job, feed.time, c))
    return false;
  Sample empty{};
  for (uint8_t i = 0; i < 3; ++i)
    empty.raw[i] = static_cast<uint16_t>(c.baseline[i]);
  feed.capture(job, empty, 256);
  for (uint8_t i = 0; i < 3; ++i) {
    if (!job.captureReference(i, feed.time))
      return false;
    feed.capture(job, at(kSensorMm[i], c, 0, 0), 128);
  }
  for (uint8_t j = 0; j < c.node_count; ++j) {
    if (!job.captureNode(c.nodes[j].mm, feed.time))
      return false;
    feed.capture(job, at(c.nodes[j].mm, c, 0, 0), 128);
  }
  if (!job.beginSweep(feed.time))
    return false;
  if (check_map)
    feed.capture(job, at(c.left_mm, c, 0, 0), (c.node_count - 1) * 5);
  return true;
}
extern "C" int test_async_calibration_success_and_reuse() {
  auto c = fixture();
  HallCalibration job;
  CalibrationFeed feed;
  CHECK(prepare(job, feed, c));
  CHECK(job.status().stage == CalibrationStage::Sweep);
  // 等待10ms以上开始扫，不应把前一静止采样当缺采。
  feed.time += 50000;
  for (int i = 0; i <= 320; ++i)
    feed.capture(job, at(-41 + i * 0.25f, c, 0, 0), 1);
  CHECK(job.finishSweep(feed.time));
  CHECK(job.status().stage == CalibrationStage::Completed);
  CHECK(validateCalibration(job.candidate()));
  CHECK(job.candidate().polarity[1] == -1);
  CHECK(near(job.candidate().left_mm, -41));
  CHECK(near(job.candidate().right_mm, 39));
  HallPosition p;
  CHECK(p.load(job.candidate()));
  CHECK(p.apply(enabled(), 1, 1));
  for (int i = 0; i <= 80; ++i)
    CHECK(near(p.process(at(-41 + i, c, i, i * 1000)).mm, -41 + i));
  return 0;
}
extern "C" int test_calibration_cancel_timeout_noise_coverage_keeps_old() {
  auto c = fixture();
  HallModule module;
  CHECK(module.apply(enabled(), 1));
  CHECK(module.load(c));
  CHECK(module.startCalibration(11, 0, true, -41, 39));
  module.cancelCalibration();
  CHECK(module.calibrationStatus().stage == CalibrationStage::Cancelled);
  CHECK(module.calibration().valid);
  CHECK(near(module.calibration().nodes[1].mm, kSensorMm[0]));
  CHECK(module.startCalibration(12, 0xffff0000u, true, -41, 39));
  module.tick(0xffff0000u + 180000000u);
  CHECK(module.calibrationStatus().error == CalibrationError::Timeout);
  CHECK(module.calibration().valid);
  CHECK(!module.startCalibration(13, 0, false, -41, 39));
  CHECK(module.calibrationStatus().error == CalibrationError::MagnetNotRemoved);
  CHECK(module.startCalibration(14, 0, true, -41, 39));
  for (uint32_t i = 0; i < 256; ++i) {
    Sample s{};
    for (auto &r : s.raw)
      r = i % 2 ? 2010 : 2090;
    s.sequence = i;
    s.timestamp_us = i * 1000;
    module.sample(s);
  }
  CHECK(module.calibrationStatus().error == CalibrationError::Noisy);
  CHECK(module.calibration().valid);
  CalibrationFeed feed;
  CHECK(prepare(module, feed, c));
  feed.capture(module, at(-41, c, 0, 0), 1);
  CHECK(!module.finishSweep(feed.time));
  CHECK(module.calibrationStatus().error == CalibrationError::MissingCoverage);
  CHECK(module.calibration().valid);
  CHECK(module.startCalibration(15, 0, true, -41, 39));
  Sample clipped{};
  clipped.raw[0] = 4095;
  module.sample(clipped);
  CHECK(module.calibrationStatus().error == CalibrationError::Saturated);
  CHECK(module.startCalibration(16, 0, true, -41, 39));
  Sample ok{};
  for (auto &r : ok.raw)
    r = 2048;
  ok.sequence = 1;
  ok.timestamp_us = 1000;
  module.sample(ok);
  ok.sequence = 3;
  ok.timestamp_us = 2000;
  module.sample(ok);
  CHECK(module.calibrationStatus().error == CalibrationError::SamplingError);
  CHECK(module.calibration().baseline[0] == 2048);
  return 0;
}
extern "C" int test_module_calibration_success_atomic_application() {
  auto old = fixture(), changed = fixture();
  for (auto &baseline : changed.baseline)
    baseline += 10;
  changed.gain[0] = 1.25f;
  changed.gain[1] = 0.8f;
  changed.gain[2] = 1.1f;
  HallModule module;
  CHECK(module.apply(enabled(), 1));
  CHECK(module.load(old));
  CalibrationFeed feed;
  CHECK(prepare(module, feed, changed));
  CHECK(module.calibration().baseline[0] == 2048); // 候选不污染旧有效快照。
  for (int i = 0; i <= 320; ++i)
    feed.capture(module, at(-41 + i * 0.25f, changed, 0, 0), 1);
  CHECK(module.finishSweep(feed.time));
  CHECK(module.calibrationStatus().stage == CalibrationStage::Completed);
  CHECK(module.calibration().baseline[0] == 2048); // 完成命令仍等下一采样边界。
  feed.capture(module, at(20, changed, 0, 0), 1);
  CHECK(module.calibration().baseline[0] == 2058);
  CHECK(validateCalibration(module.calibration()));
  CHECK(module.resume(1));
  feed.capture(module, at(20, changed, 0, 0), 1);
  Estimate e;
  CHECK(module.tryTake(e));
  CHECK(e.valid && near(e.mm, 20));
  CHECK(!e.active); // 校准恢复不发旧活动/相对历史。
  return 0;
}
extern "C" int test_module_queue_recovery_revision_calibration_suppression() {
  auto c = fixture();
  HallModule m;
  CHECK(m.apply(enabled(), 7));
  CHECK(m.load(c));
  CHECK(m.resume(10));
  for (uint32_t i = 0; i < 65; ++i)
    m.sample(at(i % 2 ? 30 : -30, c, i, i * 1000));
  CHECK(m.paused() && m.recoveryNeeded());
  CHECK(m.statistics().queue_full == 1);
  CHECK(!m.latest().valid);
  Estimate suppressed;
  CHECK(!m.tryTake(suppressed));
  CHECK(m.discardPaused() == 64);
  CHECK(m.resume(11, 64));
  CHECK(m.statistics().recovery_discarded == 64);
  m.sample(at(12, c, 65, 65000));
  Estimate e;
  CHECK(m.tryTake(e));
  CHECK(e.valid && near(e.mm, 12) && e.epoch == 11 && e.revision == 7 &&
        !e.active);
  CHECK(!m.tryTake(e));
  CHECK(!m.apply(enabled(), 8));
  m.pause();
  CHECK(m.apply(enabled(), 8));
  CHECK(!m.resume(11));
  CHECK(m.resume(12));
  CHECK(!m.startCalibration(33, 66000, true, -41, 39));
  m.pause();
  CHECK(m.discardPaused() == 0);
  CHECK(m.startCalibration(33, 66000, true, -41, 39));
  CHECK(m.resume(13));
  Sample empty{};
  for (auto &r : empty.raw)
    r = 2048;
  empty.sequence = 66;
  empty.timestamp_us = 66000;
  m.sample(empty);
  CHECK(m.tryTake(e));
  CHECK(!e.valid && e.quality == Quality::Calibrating);
  m.cancelCalibration();
  m.sample(at(-25, c, 67, 67000));
  CHECK(m.tryTake(e));
  CHECK(e.valid && near(e.mm, -25) && e.revision == 8 && e.epoch == 13 &&
        !e.active);
  CHECK(m.statistics().samples == 68);
  return 0;
}
struct FakeAdc : AdcReader {
  uint32_t clock = 0, delay = 4, calls = 0;
  int failed = -1;
  bool bad_raw = false;
  uint32_t nowUs() const override { return clock; }
  bool read12(uint8_t channel, uint16_t &raw) override {
    ++calls;
    clock += delay;
    raw = bad_raw ? 6000 : 2000 + channel;
    return failed != channel;
  }
};
extern "C" int test_sampler_budget_cadence_errors_timestamp_wrap() {
  HallSampler sampler;
  FakeAdc adc;
  Sample sample;
  adc.clock = 0xfffffff8u;
  CHECK(sampler.poll(adc.clock, adc, sample));
  CHECK(sample.ok && sample.sequence == 0 && sample.span_us == 12);
  CHECK(sample.timestamp_us == 0u);
  CHECK(sample.channel_offset_us[2] == 12);
  CHECK(!sampler.poll(100, adc, sample));
  CHECK(adc.calls == 3);
  adc.clock = 992;
  CHECK(sampler.poll(adc.clock, adc, sample));
  CHECK(sample.sequence == 1);
  adc.clock = 5000;
  adc.failed = 1;
  CHECK(sampler.poll(adc.clock, adc, sample));
  CHECK(!sample.ok);
  CHECK(sampler.statistics().errors == 1 &&
        sampler.statistics().deadline_misses == 1);
  adc.clock = 6000;
  adc.failed = -1;
  adc.delay = 100;
  CHECK(sampler.poll(adc.clock, adc, sample));
  CHECK(!sample.ok);
  CHECK(sampler.statistics().max_span_us == 300);
  adc.clock = 7000;
  adc.delay = 4;
  adc.bad_raw = true;
  CHECK(sampler.poll(adc.clock, adc, sample));
  CHECK(!sample.ok);
  CHECK(sampler.statistics().errors == 3);
  CHECK(sampler.statistics().attempts == 5);
  return 0;
}
extern "C" int test_measured_short_travel_and_maximum_nodes() {
  auto c = fixture();
  c.left_mm = -30;
  c.right_mm = 30;
  c.node_count = 9;
  c.sweep_segments = 255;
  const float labels[9] = {
      -30, -25, kSensorMm[0] * 0.5f, -8, 0, 8, kSensorMm[2] * 0.5f, 25, 30};
  for (uint8_t j = 0; j < 9; ++j) {
    c.nodes[j].mm = labels[j];
    for (uint8_t i = 0; i < 3; ++i)
      c.nodes[j].response[i] =
          100 +
          900 * clamp(1 - magnitude(labels[j] - kSensorMm[i]) / 33.75f, 0, 1);
  }
  CHECK(validateCalibration(c));
  HallPosition p;
  CHECK(p.load(c));
  CHECK(p.apply(enabled(), 1, 1));
  CHECK(near(p.process(at(-30, c, 0, 0)).p, 0));
  CHECK(near(p.process(at(30, c, 1, 1000)).p, 1));
  HallCalibration task;
  CHECK(task.start(12, 0, true, -30, 30));
  task.cancel();
  // 17节点边界不越界；保留5个关键位置，以线性三维曲线验证最大容量。
  c = fixture();
  c.node_count = 17;
  c.left_mm = -45;
  c.right_mm = 45;
  c.sweep_segments = 65535;
  const float large[17] = {
      -45, -40, kSensorMm[0],        -28, -23, kSensorMm[0] * 0.5f, -11, -5, 0,
      5,   11,  kSensorMm[2] * 0.5f, 23,  28,  kSensorMm[2],        40,  45};
  for (uint8_t j = 0; j < 17; ++j) {
    c.nodes[j].mm = large[j];
    for (uint8_t i = 0; i < 3; ++i) {
      c.nodes[j].response[i] = 600 + (i + 1) * large[j] * 5;
      c.nodes[j].noise[i] = 0.5f;
    }
  }
  CHECK(validateCalibration(c));
  CHECK(p.load(c));
  for (uint32_t i = 0; i <= 180; ++i)
    CHECK(
        near(p.process(at(-45 + i * 0.5f, c, i, i * 1000)).mm, -45 + i * 0.5f));
  auto malformed = c;
  malformed.node_count = 18;
  CHECK(project(c.nodes[0].response, malformed).ambiguous);
  return 0;
}

extern "C" int
test_calibration_control_cooperative_checks_and_command_timeout() {
  auto c = fixture();
  HallCalibration job;
  CalibrationFeed feed;
  CHECK(prepare(job, feed, c, false));
  CHECK(job.status().stage == CalibrationStage::MapCheck);
  CHECK(job.status().collected == 0 && job.status().required == 30);
  feed.capture(job, at(-41, c, 0, 0), 1);
  CHECK(job.status().collected == 1 &&
        job.status().stage == CalibrationStage::MapCheck);
  CHECK(!job.finishSweep(feed.time));
  feed.capture(job, at(-41, c, 0, 0), 29);
  CHECK(job.status().stage == CalibrationStage::Sweep);
  for (int i = 0; i <= 320; ++i)
    feed.capture(job, at(-41 + i * 0.25f, c, 0, 0), 1);
  CHECK(!job.finishSweep(feed.time + 180000000u));
  CHECK(job.status().error == CalibrationError::Timeout);
  CHECK(job.start(2, 0, true, -41, 39));
  Sample baseline{};
  for (auto &raw : baseline.raw)
    raw = 2048;
  baseline.sequence = 1;
  baseline.timestamp_us = 1000;
  job.sample(baseline);
  job.sample(baseline);
  CHECK(job.status().collected == 1);
  job.cancel();
  CHECK(job.start(3, 0, true, -41, 39));
  feed = {};
  feed.capture(job, baseline, 256);
  CHECK(!job.captureReference(1, feed.time));
  CHECK(job.captureReference(0, feed.time));
  baseline.raw[0] = 2058;
  feed.capture(job, baseline, 128);
  CHECK(job.status().error == CalibrationError::WeakReference);
  CHECK(!job.candidate().valid);
  return 0;
}

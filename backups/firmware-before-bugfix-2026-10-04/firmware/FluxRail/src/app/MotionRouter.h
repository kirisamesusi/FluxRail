#pragma once
#include "../common/MotionTypes.h"
namespace fluxrail {
namespace app {
class MotionSink {
public:
  virtual bool relative(int16_t x, int16_t y) = 0;
  virtual bool absolute(float output, uint32_t revision, uint32_t epoch) = 0;
  virtual ~MotionSink() = default;
};
// 终身软件账本；configure清流而不清历史计数。submitted并非游戏/主机接收证明。
// discarded保留方向净和，discarded_absolute用于识别往返损失，二者不能互换。
struct RoutingStats {
  uint32_t stale = 0, invalid = 0, switches[2]{}, hall_superseded = 0,
           queue_full = 0;
  double submitted[2]{}, discarded[2]{}, discarded_absolute[2]{};
};
// 核心0唯一运动路由。每轴保序分时段，独立signed16额度，busy绝不扣未发送量。
class MotionRouter {
public:
  void configure(const common::RoutingSettings &, uint32_t revision,
                 uint32_t epoch, bool io4);
  void clear();
  bool canAccept(knobs::Axis axis) const {
    return queues_[uint8_t(axis)].count < 64;
  }
  void hall(const hall::Estimate &, uint32_t now);
  void knob(const knobs::Motion &, uint32_t now);
  void service(MotionSink &);
  const RoutingStats &stats() const { return stats_; }
  bool faulted() const { return overflow_; }
  uint8_t owner(uint8_t axis) const { return axis < 2 ? owners_[axis] : 0; }
  double pending(uint8_t axis) const;

private:
  struct Queue {
    double values[64]{};
    uint8_t head = 0, count = 0;
    double fraction[2]{};
  };
  Queue queues_[2];
  common::RoutingSettings settings_{};
  uint32_t revision_ = 0, epoch_ = 0, last_active_[2]{}, selected_at_[2]{};
  uint8_t owners_[2]{}; // 0无，1Hall，2U4，3U5。
  bool io4_ = false, have_absolute_ = false, absolute_turn_ = true,
       overflow_ = false;
  float absolute_ = 0;
  RoutingStats stats_{};
  void dropAxis(uint8_t axis);
  void choose(uint8_t axis, uint8_t source, uint32_t now, bool active);
  static double magnitude(double v) { return v < 0 ? -v : v; }
  int16_t next(uint8_t axis);
  void subtract(uint8_t axis, int16_t sent);
};
} // namespace app
} // namespace fluxrail

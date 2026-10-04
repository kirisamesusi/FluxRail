#pragma once
#include "KnobMotion.h"
#include "PushFilter.h"
#include "../realtime/SpscQueue.h"
#include <atomic>

namespace fluxrail {
namespace knobs {
// 仅核心1调用；适配器应转调App::submitFilteredPush，禁止自己写十二路SPSC。
// false表示公共层请求停流恢复；公共InputWorker仍持有当前真实PUSH状态。
class PushSink {
public:
  virtual ~PushSink() = default;
  virtual bool submitFilteredPush(uint8_t source_id, bool down, uint32_t now_us) = 0;
};
// 清旧队列的数量账本，不把快速正反的净和零当无损失证明。
struct Discarded {
  uint32_t records = 0;
  double counts = 0, absolute_counts = 0;
};
struct ModuleStats {
  uint32_t queue_full[kKnobCount]{}, recovery_discarded[kKnobCount]{};
  uint32_t paused_samples[kKnobCount]{}, push_submit_failures = 0;
  double recovery_discarded_counts[kKnobCount]{}, recovery_discarded_absolute_counts[kKnobCount]{};
};
// 模块内边界缓冲使用公共SpscQueue实现，每路64记录，保留快速正反时间顺序。
// 无USB/存储/协议所有权；不是重复定义的公共MotionRecord或公共配置。
// 核心1: apply/pause/resume/sample/latest/stats；核心0: tryTake/paused/recoveryNeeded/discardPaused。
// latest/stats引用不跨核共享；诊断需由核心1复制到公共快照。
class KnobModule {
public:
  bool apply(const Settings (&settings)[kKnobCount], uint32_t revision);
  void pause(PushSink &sink, uint32_t now_us);
  bool resume(uint32_t new_epoch, const Discarded (&discarded)[kKnobCount]);
  void sample(uint8_t knob_id, const Sample &sample, PushSink &sink);
  bool tryTake(uint8_t knob_id, Motion &motion);
  bool paused(uint8_t knob_id) const;
  bool recoveryNeeded(uint8_t knob_id) const;
  // 核心0收到生产者暂停ACK后才调用；单纯读paused不能替代公共控制握手。
  Discarded discardPaused(uint8_t knob_id);
  const Motion &latest(uint8_t id) const { return latest_[id]; } // 调用者必须id<2。
  const PushSnapshot &latestPush(uint8_t id) const { return push_latest_[id]; }
  const MotionStats &motionStats(uint8_t id) const { return motion_[id].stats(); }
  const PushStats &pushStats(uint8_t id) const { return push_[id].stats(); }
  const ModuleStats &stats() const { return stats_; }
private:
  KnobMotion motion_[kKnobCount]{};
  PushFilter push_[kKnobCount]{};
  realtime::SpscQueue<Motion, 64> queues_[kKnobCount]{};
  std::atomic<bool> paused_[kKnobCount]{{true}, {true}};
  std::atomic<bool> drained_[kKnobCount]{{true}, {true}};
  std::atomic<bool> recovery_[kKnobCount]{{false}, {false}};
  Motion latest_[kKnobCount]{};
  PushSnapshot push_latest_[kKnobCount]{};
  ModuleStats stats_{};
  uint32_t epoch_ = 0, revision_ = 0;
};
} // namespace knobs
} // namespace fluxrail

#pragma once
#include "KnobTypes.h"

namespace fluxrail {
namespace knobs {
// 核心1候选防误触：GPIO和CRC有效Mg[2]一致、健康场、短稳定确认。
// 旋转只延长按下确认，不一律屏蔽；无法区分的机械误触必须靠实板评估。
class PushFilter {
public:
  void apply(const PushSettings &settings);
  void reset(); // 输出结束并要求重新看到稳定低，启动已按不制造DOWN。
  bool sample(const Sample &sample, bool rotating);
  bool down() const { return down_; }
  const PushStats &stats() const { return stats_; }
private:
  PushSettings settings_{};
  PushStats stats_{};
  uint32_t low_since_ = 0, candidate_since_ = 0, held_since_ = 0;
  bool down_ = false, armed_ = false, low_seen_ = false;
  bool candidate_ = false, candidate_rotating_ = false, mismatch_ = false;
  bool releasing_ = false, sampled_ = false;
  uint32_t last_sample_us_ = 0, last_sequence_ = 0;
};
} // namespace knobs
} // namespace fluxrail

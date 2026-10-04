#include "MotionRouter.h"
namespace fluxrail {
namespace app {
// 只在来源/epoch/维护切换时显式舍弃未提交量。净值与绝对量分别记账，
// 快速正反净和为零也不能被解释为没有丢弃；绝对X只清X所属待发坐标。
void MotionRouter::dropAxis(uint8_t a) {
  auto &q = queues_[a];
  for (double f : q.fraction) {
    stats_.discarded[a] += f;
    stats_.discarded_absolute[a] += magnitude(f);
  }
  for (uint8_t i = 0; i < q.count; ++i) {
    double n = q.values[(q.head + i) % 64];
    stats_.discarded[a] += n;
    stats_.discarded_absolute[a] += magnitude(n);
  }
  q = {};
  if (a == 0)
    have_absolute_ = false;
}
void MotionRouter::clear() {
  for (uint8_t a = 0; a < 2; ++a) {
    dropAxis(a);
    owners_[a] = 0;
    last_active_[a] = 0;
    selected_at_[a] = 0;
  }
}
void MotionRouter::configure(const common::RoutingSettings &s, uint32_t r,
                             uint32_t e, bool io4) {
  clear();
  settings_ = s;
  revision_ = r;
  epoch_ = e;
  io4_ = io4;
  overflow_ = false;
}
// 一个轴只有一个运动所有者，接管时清该轴旧余量，另一轴继续独立工作。
// now的差值采用uint32自然回绕；selected_at用于同一服务批次的确定性平局。
void MotionRouter::choose(uint8_t a, uint8_t source, uint32_t now,
                          bool active) {
  if (owners_[a] == source) {
    if (active)
      last_active_[a] = now;
    return;
  }
  dropAxis(a);
  owners_[a] = source;
  selected_at_[a] = now;
  last_active_[a] = active ? now : now - settings_.idle_us;
  ++stats_.switches[a];
}
// Hall记录已经完成校准、gain/invert和范围映射；公共路由只选择来源。
// 无效/旧版本不发布坐标；最新绝对值允许覆盖待发值并计superseded，
// 此策略适合绝对位置，不能用于相对旋钮的分时增量。
void MotionRouter::hall(const hall::Estimate &m, uint32_t now) {
  if (m.revision != revision_ || m.epoch != epoch_) {
    ++stats_.stale;
    return;
  }
  if (!m.valid || !(m.output >= 0 && m.output <= 1)) {
    ++stats_.invalid;
    if (owners_[0] == 1) {
      have_absolute_ = false;
      owners_[0] = 0;
    }
    return;
  }
  // IO4没有旋钮扩展；直接接已配置绝对值，不写spinner、不二次倍率。
  if (io4_) {
    choose(0, 1, now, m.active);
  } else if (owners_[0] == 0 ||
             (m.active && (owners_[0] == 1 ||
                           now - last_active_[0] >= settings_.idle_us))) {
    choose(0, 1, now, m.active);
  }
  if (owners_[0] != 1)
    return;
  if (m.active)
    last_active_[0] = now;
  if (have_absolute_)
    ++stats_.hall_superseded;
  absolute_ = m.output;
  have_absolute_ = true;
}
// 背景来源的增量计入策略丢弃，不把离线/未接管历史补发成突然跳动。
// 当前来源增量保序入每轴64段FIFO；满队列明确fault，App随后申请新epoch恢复。
void MotionRouter::knob(const knobs::Motion &m, uint32_t now) {
  const uint8_t a = uint8_t(m.axis);
  if (a > 1 || m.knob_id > 1 || m.revision != revision_ || m.epoch != epoch_) {
    ++stats_.stale;
    return;
  }
  if (!m.valid || !m.enabled ||
      !(m.delta_counts >= -1e12 && m.delta_counts <= 1e12)) {
    ++stats_.invalid;
    return;
  }
  const uint8_t source = uint8_t(m.knob_id + 2);
  bool take = !owners_[a] || owners_[a] == source;
  if (m.active && now - last_active_[a] >= settings_.idle_us)
    take = true;
  // 同批起动的优先级覆盖idle_us=0；避免零滞回破坏Hall优先。
  if (a == 0 && owners_[0] == 1 && m.active && selected_at_[0] == now)
    take = !settings_.simultaneous_hall_first;
  if (owners_[a] >= 2 && owners_[a] != source && selected_at_[a] == now)
    take = false; // 同轴同批先动者持有。
  if (!io4_ && take)
    choose(a, source, now, m.active);
  if (io4_ || owners_[a] != source) {
    stats_.discarded[a] += m.delta_counts;
    stats_.discarded_absolute[a] += magnitude(m.delta_counts);
    return;
  }
  if (m.active)
    last_active_[a] = now;
  auto &q = queues_[a];
  if (m.delta_counts != 0 && q.count == 64) {
    ++stats_.queue_full;
    overflow_ = true;
    stats_.discarded[a] += m.delta_counts;
    stats_.discarded_absolute[a] += magnitude(m.delta_counts);
    return;
  }
  if (m.delta_counts != 0) {
    q.values[(q.head + q.count) % 64] = m.delta_counts;
    ++q.count;
  }
}
// 生成本轮signed16额度：不足一个count的段转入同符号fraction。
// 只有发送成功才subtract；next可整理小数段但不会扣除可发送整数，
// USB busy重试仍保留同一段剩余量，正反段不会合并成净和而丢时间顺序。
int16_t MotionRouter::next(uint8_t a) {
  auto &q = queues_[a];
  // 每轴至多64个分时段，正/负小数分别累计，反向小数不能吞掉下一完整整数段。
  for (uint8_t budget = 0; budget < 64 && q.count; ++budget) {
    const uint8_t sign = q.values[q.head] < 0 ? 1 : 0;
    double v = q.values[q.head] + q.fraction[sign];
    if (v >= 1 || v <= -1) {
      q.values[q.head] = v;
      q.fraction[sign] = 0;
      // 同向连续段可合成一份USB额度；反向段必须保序，不能抵消成净和。
      // 这里只整理队列，不扣量；提交失败时合并后的完整量仍保留。
      for (uint8_t merge = 0; merge < 63 && q.count > 1 &&
           v < 32767 && v > -32767; ++merge) {
        const uint8_t index = uint8_t((q.head + 1) % 64);
        const double following = q.values[index];
        if ((following < 0) != (v < 0))
          break;
        v += following;
        q.values[index] = v;
        q.head = index;
        --q.count;
      }
      return v > 32767 ? 32767 : v < -32767 ? -32767 : static_cast<int16_t>(v);
    }
    q.fraction[sign] = v;
    q.head = uint8_t((q.head + 1) % 64);
    --q.count;
  }
  return 0;
}
// MotionSink成功接收报告后才扣量，submitted表示软件成功提交而非主机确认。
// 尾部不足一count转回对应符号fraction，保留跨记录累计精度。
void MotionRouter::subtract(uint8_t a, int16_t n) {
  if (!n)
    return;
  auto &q = queues_[a];
  q.values[q.head] -= n;
  stats_.submitted[a] += n;
  if (q.values[q.head] < 1 && q.values[q.head] > -1) {
    q.fraction[q.values[q.head] < 0 ? 1 : 0] += q.values[q.head];
    q.head = uint8_t((q.head + 1) % 64);
    --q.count;
  }
}
void MotionRouter::service(MotionSink &sink) {
  const int16_t x = next(0), y = next(1);
  if (!x && y && have_absolute_ &&
      sink.absoluteWithY(absolute_, y, revision_, epoch_)) {
    subtract(1, y);
    have_absolute_ = false;
    absolute_turn_ = false;
    return;
  }
  // 两种报告共享同一端点；交替先服务，持续Y不能饿死绝对X。
  if (absolute_turn_ && have_absolute_ &&
      sink.absolute(absolute_, revision_, epoch_)) {
    have_absolute_ = false;
    absolute_turn_ = false;
  }
  if ((x || y) && sink.relative(x, y)) {
    subtract(0, x);
    subtract(1, y);
    absolute_turn_ = true;
  }
  if (have_absolute_ && sink.absolute(absolute_, revision_, epoch_)) {
    have_absolute_ = false;
    absolute_turn_ = false;
  }
}
double MotionRouter::pending(uint8_t a) const {
  if (a > 1)
    return 0;
  const auto &q = queues_[a];
  double sum = q.fraction[0] + q.fraction[1];
  for (uint8_t i = 0; i < q.count; ++i)
    sum += q.values[(q.head + i) % 64];
  return sum;
}
} // namespace app
} // namespace fluxrail

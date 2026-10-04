#pragma once
#include <atomic>
#include <stdint.h>
#include <type_traits>

namespace fluxrail {
namespace realtime {
// 仅一生产者/一消费者；单调32位索引差判断容量，支持完整计数回绕。
// volatile不提供跨核同步。release发布槽内容，acquire保证另一核读到完整对象。
// 不使用原子RMW、不占RP2040硬件FIFO；中断/第三生产者不能调用同一实例。
template <class T, uint32_t Capacity> class SpscQueue {
  static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
                "Capacity must be power of two");
  static_assert(Capacity < 0x80000000u, "Capacity must fit unsigned distance");
  static_assert(std::is_trivially_copyable<T>::value,
                "Queue entries must be value types");

public:
  SpscQueue() = default;
  SpscQueue(const SpscQueue &) = delete;
  SpscQueue &operator=(const SpscQueue &) = delete;
  bool tryPush(const T &value) {
    const uint32_t head = head_.load(std::memory_order_relaxed);
    const uint32_t tail = tail_.load(std::memory_order_acquire);
    if (head - tail == Capacity)
      return false;
    slots_[head & (Capacity - 1)] = value;
    head_.store(head + 1, std::memory_order_release);
    return true;
  }
  // peek允许模块背压时保留原边沿。只有本消费者能pop或clearConsumer。
  bool tryPeek(T &value) const {
    const uint32_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire))
      return false;
    value = slots_[tail & (Capacity - 1)];
    return true;
  }
  bool pop() {
    const uint32_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire))
      return false;
    tail_.store(tail + 1, std::memory_order_release);
    return true;
  }
  bool tryPop(T &value) {
    if (!tryPeek(value))
      return false;
    return pop();
  }
  // 维护/配置切换必须先拿到生产者暂停ACK再调用。返回明确策略丢弃数量。
  uint32_t clearConsumer() {
    const uint32_t head = head_.load(std::memory_order_acquire);
    const uint32_t tail = tail_.load(std::memory_order_relaxed);
    tail_.store(head, std::memory_order_release);
    return head - tail;
  }

private:
  alignas(4) std::atomic<uint32_t> head_{0};
  alignas(4) std::atomic<uint32_t> tail_{0};
  T slots_[Capacity]{};
};
} // namespace realtime
} // namespace fluxrail

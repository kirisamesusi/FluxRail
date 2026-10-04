#pragma once
#include "../common/MotionTypes.h"
#include "SpscQueue.h"

namespace fluxrail {
namespace realtime {
enum class ControlOp : uint8_t { PauseApply, Resume };
struct ConfigCommand {
  ControlOp op = ControlOp::PauseApply;
  common::RuntimeConfig config{};
  uint32_t epoch = 0;
  common::AcquisitionConfig acquisition{};
};
// 生产/消费回调都在既有控制命令ACK边界调用；可选空指针保留模块1隔离测试。
class AcquisitionHook {
public:
  virtual void pause(uint32_t now_us) = 0; // 核心1，必须在暂停ACK前停止发布。
  virtual bool resume(const common::AcquisitionConfig &, uint32_t revision,
                      uint32_t epoch,
                      uint32_t now_us) = 0; // 核心1，实际采用后才ACK。
  virtual void discardPaused() = 0; // 核心0，仅收到暂停ACK后，Resume前。
  virtual ~AcquisitionHook() = default;
};
// 核心0 -> 核心1传完整不可变快照，核心1 -> 核心0回复采样边界的真实状态。
// 内容绝不共享一个正在写的Config对象；暂满时返回false重试，不等待另一核。
struct ConfigMailbox {
  SpscQueue<ConfigCommand, 4> commands;
  SpscQueue<common::InputSnapshot, 4> replies;
};
struct RealtimeBridge {
  SpscQueue<common::InputEdge, 128> edges;
  ConfigMailbox config;
  std::atomic<uint32_t> physical_mask{0}; // 只作灯/诊断，不能替代可靠边沿。
  std::atomic<uint32_t> sample_heartbeat_us{0};
  std::atomic<uint32_t> producer_fault_epoch{0};
  std::atomic<uint32_t> producer_overflows{0};
};
} // namespace realtime
} // namespace fluxrail

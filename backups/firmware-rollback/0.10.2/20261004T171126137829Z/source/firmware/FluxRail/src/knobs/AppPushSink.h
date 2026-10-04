#pragma once
#include "KnobModule.h"

namespace fluxrail {
namespace knobs {
// 公共PUSH入口的薄适配器：生产代码用AppPushSink<app::App>，绑定唯一App实例。
// 模板避免业务头文件依赖USB/协议整套App定义；绝不分配InputEdge序号/另写输入队列。
// 仅核心1，在App::beginCore1完成后服务模块；对象和App必须长于模块调用寿命。
template<class AppType> class AppPushSink final : public PushSink {
public:
  explicit AppPushSink(AppType &app) : app_(app) {}
  bool submitFilteredPush(uint8_t source_id, bool down, uint32_t now_us) override {
    return app_.submitFilteredPush(source_id, down, now_us);
  }
private:
  AppType &app_;
};
} // namespace knobs
} // namespace fluxrail

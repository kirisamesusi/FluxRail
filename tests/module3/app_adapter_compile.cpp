// 仅严格编译核对真实App类型；不建立第二个setup/loop、不运行硬件。
#include "app/App.h"
#include "knobs/AppPushSink.h"
bool compile_actual_app_push(fluxrail::app::App &app, uint32_t now) {
  fluxrail::knobs::AppPushSink<fluxrail::app::App> sink(app);
  return sink.submitFilteredPush(10, false, now);
}

#include <type_traits>
static_assert(std::is_same<fluxrail::common::KnobSettings, fluxrail::knobs::Settings>::value,
              "Public profile must reuse module settings");
bool compile_public_profile(fluxrail::knobs::KnobModule &module,
                            const fluxrail::common::Profile &profile, uint32_t revision) {
  return module.apply(profile.knobs, revision);
}

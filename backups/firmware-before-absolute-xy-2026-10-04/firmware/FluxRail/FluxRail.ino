// FluxRail唯一正式入口。模块逻辑、USB初始化/回调与跨核状态机均在src分目录。
// 当前普通NKRO+鼠标三按钮集成原型；尚无Flash/CDC命令/模块2/3/IO4实现。
#include "src/app/App.h"
bool core1_separate_stack =
    true; // Arduino-Pico为核心1分配独立8KiB栈，避免两核共用小栈。
void setup() { fluxrail::app::application().beginCore0(); }
void loop() { fluxrail::app::application().serviceCore0(); }
void setup1() { fluxrail::app::application().beginCore1(); }
void loop1() { fluxrail::app::application().serviceCore1(); }

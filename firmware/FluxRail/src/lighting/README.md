> 2026-10-05现行交接：[模块文档](../../../../docs/modules/01_按键与灯光.md)、[测试状态](../../../../docs/测试状态与待验收.md)。本模块已接入正式App；下文为原模块阶段技术说明，旧“未接入”与测试范围按历史日期阅读。原始字节已保存于整理前文档压缩包。

# 模块1：灯光接入说明

本目录负责L01～L04。核心0生成帧并调用唯一灯输出；没有USB或IO4灯命令解析。公共配置/协议层只能递交参数或LightFrame，不能绕过packLightWords直接写硬件。

## 帧与配置

renderLights(config, physical_mask, now_ms, indicator_enabled, effective_indicator_color)纯算法，建议公共调度约60Hz调用。十键颜色按物理BT1..BT10保存；PUSH没有新增灯，不根据映射后usage决定PRESS亮灯。四持续效果Static/Breathing/ColorCycle/RainbowWave，另有Press和enabled关闭。Breathing采用轻量三角包络；自动色只产生输出，不覆盖key_colors。RainbowWave按左右ASDFV/JKL;N布局排色。

brightness_level只有0..100，floor(channel*envelope*level/(255*200))；100实际50%，输出最大127；50实际25%，白色得到63。global_color/key_colors是0..255原始RGB，per_key选用逐BT配色。period_ms=100..60000，reverse只影响动画相位。默认Press、brightness50、白色、2000ms均是D04工程起点，公共预置可覆盖。非法配置键灯输出黑色。

GP11输入颜色由公共层按实际生效模板/USB身份选择；本模块不决定D06色表/优先级。indicator_enabled独立于键灯开关和亮度，固定floor(RGB/4)，通道最大63，实际25%。公共层如需整机全关，应同时关闭两组。

LightFrame keys是BT顺序；LedOrder.chain_to_bt是每个串联灯的物理来源索引。默认{8,7,6,5,4,3,2,1,0,9}对应BT9/8/7/6/5/4/3/2/1/10，用户于2026-10-03确认此灯链顺序；尚未记录实板点灯结果。只有0..9完整排列可用；非法顺序输出十灯黑色，begin拒绝。grb默认true也待逐灯/色序实测，可改false为RGB。packLightWords对所有外来帧再次限幅127/63，输出24位数据左对齐的32位DMA字。

## RP2040资源与生命周期

Ws2812OutputRp2040只能由核心0一个长期存活实例持有，不可复制；没有自动析构清外设。begin动态申请同一PIO上的2个SM，共享4条程序指令及2个DMA通道；GP10十灯、GP11一灯并行。资源申请失败回收并返回false。集成层需在资源表安排，不能让其他模块并行修改同一实例；begin/end为维护阶段资源操作，不属于周期服务耗时保证。

PIO800kHz，每bit十周期，0高2/低8、1高7/低3。DMA为32位递增读、固定PIO TX写，FIFO join TX=8；左移/autopull24。时钟分频按begin时clk_sys计算，后续若改CPU时钟必须停机重建。

trySubmit先service；忙返回false，不覆盖DMA源。公共灯调度可保留最新一帧下次重试，丢旧动画帧可接受，不能占用按键可靠队列。DMA结束不等于最后bit已输出，额外等待570us（FIFO8+OSR1最多270us+低电平锁存300us）后放行；等待由时间状态机完成，没有delay/关中断show/循环等DMA。

DMA仍busy超过5ms时锁定fault、计timeouts，缓冲不可改写；公共健康层需受控重启/外设停机处理。end仅在不busy时回收，不调用可能等待硬件的abort。不要销毁仍busy的对象。调用now_us必须取自time_us_32/micros单调时间，begin后重新取时间；周期调用间隔小于2^31us。输出统计frames/busy_rejections/max_service_us/timeouts；max_service_us只测service本身，不包含渲染/pack/trySubmit总时间。最终总耗时、中断影响、FIFO时序及电平由实板捕获验证。

所有六个翻译单元严格ARM编译，PIO/DMA还通过Arduino完整链接烟测。没有实际外设运行、逐灯、电平或USB并发测试。GP11为独立线，不当第十一颗串联灯。灯珠确切型号及3.3V直驱5V灯的适配仍待硬件验证。


## 阶段10：统一亮度及模式灯

scaleKeyColor为本地效果/IO4 GPIO共用换算，非法level黑色；外部命令必须同时遵守已确认LightingConfig.enabled与brightness_level，pack仍是最终硬上限。主机只选六ABC功能键的色，不更改模板配色，其余物理灯本地渲染。applyHostLights(frame, lighting)传入appliedConfig的灯设置，不能传尚待ACK的请求。

modeIndicatorColor只收实际身份：普通保留模板RGB，IO4品红；App首次尚未确认配置时保留默认白色，之后依旧只用最后确认的模板开关。四预置新默认青/橙/紫/绿，可通过既有indicator_color改普通显示；已有保存/迁移不覆盖。色表为工程建议，模式灯输出固定实际25%。

保存必须等!busy；faulted不表示DMA已停止，既有维护超时失败后恢复输入，不擦Flash、不重用故障缓冲。新ARM模拟覆盖主机灯0..100/关闭/非法级别、模板确认与当前身份、DMA故障保存门槛；PIO/DMA外设依然仅编译，未实板运行。

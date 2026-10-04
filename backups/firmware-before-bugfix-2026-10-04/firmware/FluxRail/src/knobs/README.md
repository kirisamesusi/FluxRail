# 模块3：双MT6701相对旋钮与PUSH

2026-10-03。对应R01～R05、K03/K05/P01。本目录为核心1模块，未修改App/common/USB/Flash/CDC。进度及最新验证见docs/progress/模块3.md；实板步骤见tests/module3/HARDWARE_CHECKLIST.md。

## 当前交付边界

- 两SSI完整24位读取、CRC/场状态、独立500us采样起点、角度展开、小数相对count、原始/倍率累计圈数。
- 两PUSH的候选防误触、原始与过滤诊断；AppPushSink直接转调唯一App::submitFilteredPush(10/11,down,time)。不拥有InputEdge序号、公共输入队列或动作绑定。
- 每路64条有界记录用公共SpscQueue保留时间顺序；Motion是算法边界值，不是另一套公共MotionRecord。收尾时公共v0.7已用common::KnobSettings=knobs::Settings直接复用参数，Profile::knobs[2]可直接传apply，没有复制配置结构。公共实时运动类型尚未冻结，由集成适配器转换，接入后避免多级队列造成延迟。
- 未挂正式App调度；公共旋钮类型已由v0.7/schema1直接复用，配置/序列化代码由集成窗口实现；两核旋钮采样应用、来源接管、HID运动发送、Web和机械验收仍未完成。轴枚举和参数可以驱动算法，网页尚不能据此宣称可配置/保存。

## 文件与核心所有权

| 文件 | 职责 |
| --- | --- |
| KnobTypes.h/.cpp | 字段、完整帧CRC/状态解码、参数整包校验及默认建议 |
| Mt6701SsiRp2040.h/.cpp | SPI1 U4/SPI0 U5，RX/SCK复用与手动CS，无TX/MOSI；有界FIFO/故障停止 |
| KnobSampler.h/.cpp | 每路独立采样尝试与序号，不补采错过的时刻 |
| KnobMotion.h/.cpp | 跨零展开、可信速度边界、过滤/活动/诊断圈数及坏帧恢复 |
| PushFilter.h/.cpp | 稳定低武装、健康场与GPIO/SSI一致确认、旋转确认、UP/超时/断采恢复 |
| KnobModule.h/.cpp | 公共队列实现适配、两轴独立背压、停流/清理/新epoch、丢弃账本 |
| AppPushSink.h | 薄模板适配器，绑定唯一App，不引入USB头文件/所有者 |

核心1独占外设、sample/apply/pause/resume/latest/latestPush/stats。核心0只消费tryTake、查询原子paused/recoveryNeeded、在公共暂停ACK后discardPaused。latest/statistics返回核心1对象引用，**禁止核心0直接读取**；公共遥测需由核心1复制成值，再经统一诊断通道发布。所有状态机不分配动态内存、无Serial/Mouse/Flash调用或第二个setup/loop。

## 字段、单位及建议范围

公共schema1已复用这些字段/校验范围；默认数值仍为未完成机械验收的工程起点，后续参数/格式变化由公共契约与迁移统一管理。两个Settings整包校验后才能应用，NaN/Inf/越界拒绝，revision必须非0。

| Settings字段 | 单位/校验范围 | 默认建议及含义 |
| --- | --- | --- |
| enabled | bool | false；由模板设置，音击两路必须默认关 |
| axis | X/Y | U4=X、U5=Y；同轴时仍保留来源，不自行合并 |
| invert | bool | false，仅反转输出/倍率圈数，不改raw角度与活动阈值 |
| gain | 0.01～16 | 1；基准倍率，无默认速度加速 |
| counts_per_turn_at_gain1 | 1～1000000 count/圈 | 1024；HID count不是屏幕像素 |
| filter_tau_us | 0～20000us | 0，直接跟随；选非0时在展开后滤波增量并保留尾量 |
| max_gap_us | 500～100000us | 2000，长缺采重建，不补历史 |
| max_turns_per_second | 0.01～1000圈/s | 100；是展开可信的物理转速上界候选，超边界拒绝，绝不是HID限速 |
| activity_ticks | 1～8191原始角LSB | 8，累积物理净偏移，与gain无关；一圈16384ticks |
| activity_hold_us | 0～1000000us | 10000，物理活动滞回 |
| push.enabled | bool | false；只启用过滤，映射仍在公共bindings[P1/P2] |
| push.press_us | 0～20000us | 2000，按下稳定确认 |
| push.rotating_press_us | press_us～50000us | 5000，候选期间曾活动便保持较长门槛 |
| push.release_us | 0～20000us | 1000，任一路释放即开始UP确认 |
| push.rearm_us | 500～100000us | 2000，启动/异常/超时后需双方健康低电平稳定武装 |
| push.max_hold_us | 1000～16000000us | 2000000，超时释放且不重复按下，必须低电平重武装 |
| push.max_sample_gap_us | 500～100000us | 2000，确认不能跨缺采/断序 |

芯片PUSH EEPROM参数没有被写入；3.3V仅读取SSI/GPIO。双确认可能延迟/拒绝部分真实按压，也不能区分GPIO和SSI同时响应的偏心误触，必须用机械记录调整。

Sample包含frame24、真实帧中点timestamp_us、每路attempt sequence、span_us、transport_ok和高有效push_gpio。Motion包含knob_id0/1、axis、revision/epoch、原始角/状态、delta_ticks、int64累计raw_ticks、double raw_turns/scaled_turns/delta_counts、valid/active/enabled/quality。PushSnapshot分别记录GPIO、CRC有效SSI PUSH、场质量、过滤状态、活动与公共source_id。

raw_turns只累计可信物理增量，失效期间运动不可重构，不假装完整机械总圈数。scaled_turns每个可信原始增量按当时gain/invert积分，保留此前诊断历史；改倍率不对过去所有圈数重新乘新倍率，也不因换轴改变raw。网页用设备累计值恢复遥测间隙，不积累相邻角度。若网页要“本次测试”从0开始，自行减启动时快照基准即可；这是显示计算，不承担实时运动。

## SSI和CRC证据边界

BoardConfig唯一引脚：U4 spi1 GP12/13/14/19，U5 spi0 GP16/17/18/20。SDK直接启用两硬件SPI，不调用Arduino SPI/SPI1.begin；没有任何TX引脚复用，GP19始终PUSH输入。集成后其他模块/第三方库禁止再begin这些总线。

候选2MHz、8bit×3、MSB，CS在24时钟期间持续低，前后各固定1us等待。CPOL1/CPHA0对应厂家空闲高、下降沿采样；实物的首位/时钟数/片选保持需逻辑分析仪确认。故障单次FIFO轮询最多100us或10000迭代，超时撤CS/禁用该SPI，下次重启外设；没有重试循环阻塞另一路。正常帧理想移位24/2MHz=12us，但软件开销/中断使实际耗时更长，不能当实测最大服务耗时。

CRC覆盖18位角度+状态，多项式x^6+x+1，MSB先行，当前零初值/无反射/无最终异或。独立长除法合成参考及各位损坏测试通过。**手册未给完整初始化向量，合成测试不证明芯片实际CRC约定；必须采真实帧验证，失败不得退化成忽略CRC。** [厂家Rev1.8，第25～27页](https://www.magntek.com.cn/upload/pdf/202407/MT6701_Rev.1.8.pdf)。Mg[1:0]=0正常/1强/2弱/3保留；Mg[2]=PUSH、Mg[3]=失跟踪，各错误位独立计数。PUSH位本身不让角度失效。

## 展开、过滤与恢复

- 首次、配置/恢复后的首帧只建立参考，valid=false且delta_counts=0。
- 差值回绕到[-8192,8191]，恰半圈拒绝。有效dt须非0且不超过max_gap，并满足“配置允许最大转速×dt<半圈”；观测位移也须在该速度边界内。
- CRC错不更新角度，短间隔可对下一有效帧补净差；该间隙内往返轨迹不可观测，不能宣称没丢有效动作。运输失败/场异常/断序直接重建；长CRC间隙也清参考。
- default tau=0保留所有可信微增量、小数和急反向。tau>0采用按真实dt的展开增量低通，尾量保留，静止继续输出至收敛；异常/改配置/维护清尾量，并记filter_discarded_ticks。非0会引入反向延迟，手感需要实测。
- enabled=false仍采可信角度/累计圈数作诊断，delta_counts=0。非当前X来源也应持续采样，公共路由只丢当前运动记录，不能把背景历史圈数作为接管补发。Y独立。
- 后端从每路delta_counts保留小数，USB忙先保留未成功提交的分时记录与余量；成功提交才扣。不同轴各自额度，不共享X+Y上限，不用clamp丢另一轴。模块没有自行发HID。

## 正式集成步骤与既有公共入口

集成窗口负责唯一App编排，下面是接口用法参考，不增加主入口：

~~~cpp
// 由唯一App核心1持有以下实例，并确保beginCore1已完成。
knobs::Mt6701SsiRp2040 ssi;
knobs::KnobSampler sampler;
knobs::KnobModule module;
knobs::AppPushSink<app::App> push(application);
// 启动/暂停采样边界：module.apply(settings, revision)，ssi.begin()。
knobs::Discarded discarded[2]{};
module.resume(public_epoch, discarded);
// 每轮都服务两路，第一路失败不early-return；读时使用当前真实时间。
for (uint8_t i = 0; i < 2; ++i) {
  knobs::Sample sample;
  if (sampler.poll(i, ssi.nowUs(), ssi, sample))
    module.sample(i, sample, push);
}
~~~

AppPushSink对真实app::App严格编译通过；App内部InputWorker合并十二路边沿并赋唯一sequence/revision/epoch、更新PUSH当前状态。sink返回false时记录失败，公共InputWorker已有停流请求和当前状态，不能盲重播DOWN。App公共输入/运动的暂停握手必须统一，不把单独调用sample当两核配置已完成。

配置/断连/维护/队列满恢复顺序：

1. 核心0提交统一暂停命令；核心1在采样边界module.pause(push,now)，结束过滤PUSH并重建运动参考；按App现有握手生成完整输入状态ACK。
2. 核心0取得真实暂停ACK后，对两路discardPaused，将返回Discarded账本保留一次，并清公共后端旧运动/整数余量。paused原子状态仅供诊断，不能替代公共控制ACK。
3. 核心1完整validate两路参数，再apply(revision)，resume新的非0/不同epoch并传Discarded账本；sampler.reset仅重置节拍，不归零尝试sequence。公共层确认两核采用新revision/epoch后才回复页面成功。
4. 重新采样首帧建立角度参考，PUSH需健康低电平重新武装。恢复不补发维护期间历史。

各路队列64条，2kHz约32ms容量，仅是缓冲上限，**正常不能等队列堆满才发送**。满队列不覆盖，仅该路暂停发布并置recoveryNeeded；另一轴继续。queue_full、未入队当前记录、恢复清理记录的数量/净count/绝对count分别统计；filter_discarded_ticks另记。核心0tryTake在paused时拒绝取旧记录。核心1暂停期间仍采角度诊断，paused_samples计数且输出抑制。

## 公共接口待调整及影响（由集成窗口冻结）

| 所需公共调整 | 对其他模块的影响 |
| --- | --- |
| 旋钮参数的两核不可变快照/实际采样ACK | v0.7 DeviceConfig::Profile已经直接复用Settings和knobs::validate，DeviceCodec逐字段编码全部参数；仍需接App核心1应用/恢复，不把仅保存参数当硬件已采用。模块4按能力读回，PUSH动作继续仅bindings[10/11] |
| 唯一旋钮MotionRecord/实时运动通道 | 携带来源、分时delta_counts、raw/scaled累计值、状态/活动/时间/sequence/revision/epoch；模块2滑轨仍绝对，公共USB独立双轴发送 |
| 唯一诊断快照和统计 | 核心1复制Sample/PushSnapshot/运动与SSI/队列统计，经公共通道序列化；模块4画圆形/圈数/原始PUSH，无另造JSON |
| 统一暂停ACK/新epoch恢复 | App配置/断连/USB故障/Flash维护同时结束输入及运动，调整App调度；恢复清旧输出，不让USB忙时重播旧圈数 |
| 路由D12/同轴D13 | X服从滑轨接管，Y独立；两个旋钮同轴保留来源，由公共层冻结策略，不擅自相加 |
| 全局模板/保存与旋钮实时维护衔接 | 公共v0.7已用同一Settings序列化，音击默认双关；接入采样后Flash维护需暂停本模块/清旧队列/重建参考。USB/CDC不宣称未挂运动能力 |

本窗口没有改公共契约文件/公共代码，避免越过用户的所属模块限制。公共v0.7更新见docs/spec/12_完整配置与Flash序列化.md，本窗口已只读核对其类型别名/全字段序列化和音击默认关，未代替集成窗口验收Flash。上述清单与docs/progress/模块3.md是后续接入需要的具体调整记录。IO4旋钮扩展未定义，不自动使用相对鼠标或spinner。

## 时间、资源和验证

核心1无长delay、动态内存或USB API。硬件两次read最坏故障等待各约100us加固定等待与重初始化；算法为固定18位CRC循环、O(1)运动/过滤和单次队列操作；清队列最多64记录，只在维护握手阶段。模块有double软件运算，实际SSI+两算法+公共输入整轮最大耗时、最大间隔和2kHz达成率仍需真实125MHz板测量。初值/时序、SSI/ADC/灯/CDC并发，以及Watchdog由集成验收。

验证脚本固定Arduino-Pico6.1.0、ARM GCC16.1.0、Unicorn2.1.3、CLI0.35.3、Generic RP2040/4MiB noFS/W25Q64JV /4/125MHz/TinyUSB3.7.7。tests/module3编译与合成指令测试覆盖实际模块.cpp；完整Arduino是隔离烟测，不是正式App固件。没有上传、实板、USB主机或游戏结果。最新检查数、资源、UF2/源码哈希见tests/module3/validation_2026-10-03.json，复现说明见tests/module3/README.md。

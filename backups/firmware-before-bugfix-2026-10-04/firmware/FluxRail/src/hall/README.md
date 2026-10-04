# 模块2：三霍尔绝对位置滑轨

2026-10-03。需求H01～H04，来源活动关联P01。实现位于本目录；详细交接见[模块2进度](../../../../docs/progress/模块2.md)。本模块已严格编译/指令模拟/独立Arduino链接，尚未接正式App，未实板/游戏验证。

## 文件和所有权

| 文件 | 职责 |
| --- | --- |
| HallAdcRp2040 | 核心1唯一ADC读者，引用现有board/BoardConfig.h的GPIO26/27/28 |
| HallSampler | 每1ms一次完整向量，三通道时刻/跨度/序号，超期不追赶补采 |
| HallTypes | 模块局部值对象、参数校验和三维曲线投影；没有Profile/公共Motion/wire格式 |
| HallPosition | 绝对mm/p/配置输出、失效、实际dt滤波和物理活动 |
| HallCalibration | 空场、三参考点、7～17个已测mm标签、分时地图检查、全轨扫描、取消/超时 |
| HallModule | 模块采样边界控制、候选原子替换、复用公共SpscQueue和溢出停流恢复 |

所有算法/控制/校准/诊断方法由核心1单一拥有者执行。核心0只调用HallModule::tryTake、paused、recoveryNeeded、discardPaused，最后一个必须在公共暂停ACK之后。latest/statistics/calibration/calibrationStatus返回的是**核心1本地引用**，不能由核心0或USB回调直接读取。核心1将这些值复制到公共有界诊断快照，协议层再序列化。

没有USB/Mouse/Serial/Flash调用、USB描述符、配置存储或JSON协议，也没有第二个正式setup/loop。独立测试入口只在tests/module2/build生成。

## 字段、单位和工程起点

以下阈值/默认值是候选实现范围，尚未实板冻结；不是已测性能承诺。

| 对象/字段 | 单位、范围、默认与语义 |
| --- | --- |
| Sample.raw[3] | 12位ADC整数0..4095，H1/H2/H3永久有序；<=8或>=4087视为饱和起点 |
| timestamp_us/sequence | 32位微秒/尝试序号，可回绕；无效采样同样占序号 |
| channel_offset_us/span_us | 相对向量起点的每路有效转换完成时刻/整向量跨度；时间戳取有效首末样本中点 |
| Calibration.baseline/noise | 无场均值/标准差，ADC count；仅显式确认移开磁铁时采，绝不追踪静止磁铁更新 |
| gain/polarity | 三路增益0.1..10、ADC方向±1；根据三个传感器参考点学得，和invert分开 |
| left_mm/right_mm | 磁铁中心实测端点，相对H2零点，支持-200..200mm；不默认约±40mm |
| nodes | 7～17个严格递增mm标签，最小间距0.5mm、最大20mm；标签噪声及有符号响应 |
| noise_mm | 响应噪声/局部斜率推算值0..5mm；用于活动门槛，**不是实测位置误差** |
| Settings.enabled/invert | 默认false/false；公共模板决定开关；无有效校准始终不提交新位置 |
| center_gain | 0.1..8，默认1；q=clamp(0.5+gain*(oriented_p-0.5),0,1) |
| range_min/max | 后端无关的归一化0..1，min<max，默认0/1；公共层转换成HID/IO4范围 |
| quiet_tau_us/moving_tau_us | 0..20000us/0..quiet，默认1500/0；alpha=dt/(dt+tau)，速度连续过渡 |
| fast_speed_mm_s | 1..5000mm/s，默认200；决定滤波过渡，不是硬限速 |
| activity_mm/hold_us | 0.01..5mm/0..100000us，默认0.2mm/10000us；门槛取max(activity_mm,3*noise_mm) |
| enter_signal/exit_signal | 正响应和，默认80/60 ADC count，1<=exit<=enter<=12000，有效性滞回 |
| curve_tolerance | 三维响应离折线距离，1..1000 count，默认35；校准扫验证暂用35 |
| max_gap_us | 1000..100000us，默认10000；长缺采/断序重建参考 |
| Estimate.mm/raw_mm | 时间滤波绝对位置/未滤波折线投影，mm；不是相对增量 |
| Estimate.p/output | p=(mm-left)/(right-left)，方向/倍率无关；output含invert、中心倍率和范围 |
| centroid_mm | 扣基线且校正增益/极性的非负权重重心，仅诊断，不能当完整机械行程 |
| confidence/quality/valid | confidence=clamp(1-distance²/tolerance²,0,1)，仅拟合质量；valid决定可否提交 |
| active | 相对累积锚点的物理位移超过噪声门槛，慢移最终仍触发，不受gain影响 |
| revision/epoch | 公共层提供整体配置版本/流世代；校准/恢复后不能把旧世代结果补发 |

p保持物理定义，invert只在output中应用；集成窗口冻结公共p语义时需显式适配，不能再反转一次。gain>1提前裁边，gain<1只覆盖中央。普通HID坐标/IO4 ADC范围属于公共后端，模块不擅自决定。

有效位置来自实测三维响应折线的最近投影。不同非局部位置距最佳残差接近（差<=25 count²）且位置差>2mm时拒绝歧义，不依靠上次位置猜分支。地图每段0/25/50/75/100%检查只是有限筛查；实际磁场仍可能在节点间出现不可辨识、饱和或曲率过大，必须增加标签/改结构并实测。

无效结果携带最后有效mm/p/output用于诊断，但valid=false，公共层禁止当新绝对坐标提交。失磁/饱和/偏离曲线/缺采/改配置后清参考，恢复直接定位当前磁铁位置，首次恢复不标记虚假活动。用户已确认绝对定义，保持/立即定位具体游戏策略仍为D11候选。

## 核心1接入顺序（由公共集成窗口实施）

1. 创建唯一HallAdcRp2040/HallSampler/HallModule实例；ADC begin在核心1执行。模块初始暂停。公共层读回并校验已保存全局校准后调用load；apply(Settings,revision)，再按公共ACK完成resume(new_epoch)。同一安装上电不重新采空场。
2. 公共loop1频繁调用sampler.poll(real_now_us,adc,sample)。true时hall.sample(sample)，每次调度均hall.tick(real_now_us)，确保ADC没有数据时校准仍超时。最大调用间隔和算法耗时必须用真实时钟测量，不能只看网页。
3. 核心0先检查paused/recoveryNeeded，再有界排空tryTake；检查公共revision/epoch/时间戳，交唯一来源路由。后台模块仍采样。USB忙时保留最新有效绝对坐标及统计，不能把64条队列按相对运动累计或延迟播放。
4. 配置/维护/断连：核心1pause并回复公共ACK；核心0discardPaused取得丢弃数，清自己的旧坐标；核心1采样边界apply/load，然后resume(new_epoch,discarded)并ACK。resume必须换epoch，正常配置在校准运行时拒绝apply/load。先完整校验再更改，不能半应用。
5. 队列满：生产者不覆盖，queue_full加1，暂停发布并置recoveryNeeded。暂停仍采样/诊断；公共层获暂停ACK，清旧64条结果，按步骤4恢复当前绝对位置，明确计recovery_discarded。消费者暂停时tryTake返回false。恢复不补历史往返，溢出属于验收失败证据。

队列使用唯一公共realtime/SpscQueue<Estimate,64>，release/acquire发布；没有volatile共享快照或占用RP2040核暂停用硬件FIFO。Estimate是本模块边界结果，未来公共MotionRecord由公共层集中冻结并适配，其他模块不能复制这些结构当Profile或协议。

## 异步校准与全局复用

公共命令经同一控制邮箱转入核心1，不直接从核心0调用。返回false表示阶段/参数/忙状态拒绝，由公共层生成统一错误响应；没有新协议。

1. 按暂停ACK/清队列流程准备，然后startCalibration(job_id,now,true,left,right)。true表示用户已经把磁铁移开；算法只能验证稳定/非饱和，不能仅凭三路静态ADC证明磁铁真的已移开。成功后resume新epoch，校准期间只暂停滑轨新坐标，其他来源继续服务。
2. Baseline自动取256个有效独立样本，均值/标准差Welford在线计算。随后WaitReference：用户确认磁铁在H1/H2/H3上方，依次captureReference(0/1/2,now)，各128样本，学增益/极性。场幅<80、静态标准差>8、饱和、采样异常拒绝。
3. WaitNode：从真实左端点向右captureNode(actual_mm,now)，每标签128样本并学习静止噪声。真实右端点也必须录入；行程内的三传感器/两交接区需距标签<=2mm。行程短于传感器跨度时只检查落在实测行程内的关键位置，参考增益点必要时可在轨外同高度单独采集。
4. beginSweep(now)首先检查结构/斜率与噪声，进入MapCheck。每采样检查一个候选响应点，最多80周期；完成后状态才变Sweep，网页应等设备状态再提示开始扫。
5. Sweep：每段中间20%..80%需要实际有效采样，首末段靠端5%内也需覆盖。偏离曲线/弱场/饱和立即拒绝。扫描完整后finishSweep(now)，完成命令也检查180秒任务超时。
6. Completed只准备合格候选；HallModule在下一采样边界原子替换当前有效对象并清旧参考。失败/取消/超时保留旧对象并重新定位。全局校准由公共存储显式save保存，与所有模板共用。模块不实现CRC/Flash槽/自动保存，Calibration内存布局不可直接写盘当schema。

外部导入/开机load使用validateCalibration完整检查版本、有限数、端点、关键标签、信号/噪声、三维歧义和扫覆盖。该完整检查最多80次投影，安排在启动或已暂停阶段；实时校准采用分时检查，完成时不会重复全表扫描。人工mm标签和保存记录来源仍由用户/公共导入流程负责，无法从ADC单独推断真毫米尺度。

## 有界预算与诊断

ADC每通道换mux丢首样再取样，共6次转换。每次转换等待上限20us；启动复位/READY各100us超时。整向量跨度>250us置采样失败，这是候选预算而非实测。等待转换会短暂占核心1，但不关中断、没有delay或无期限循环；时钟/抢占仍可能增加真实服务时长。ADC时钟/电源故障须公共健康层处理，不能只以错误样本喂watchdog。

每次位置投影最多16段；MapCheck每周期一个检查点。基础/标签采集每次只在线更新3路，收满后有固定次数方差计算；beginSweep做有限结构检查和16段噪声估计。普通sample与这些边界的真实最坏耗时**未测**，1kHz尚不是承诺。公共调度需分别计ADC、定位、校准单步、命令服务和整体核心1间隔，必要时再优化/DMA。

HallSampler统计attempts/errors/deadline_misses、最大尝试间隔/向量跨度/采样服务时间；HallModule统计samples/invalid、有效性类别、最大采样/有效间隔、断序/超期、queue_full/恢复丢弃。暂停/校准的invalid属于输出抑制；类别计数以最终quality统计，不能把它当全期间独立ADC错误总数。采集错误总数应以HallSampler.errors为准。所有统计32位回绕，遥测需要按无符号差分。

核心1诊断组合Sample原始ADC、latest响应/mm/p/output/活动/质量、calibration基线/gain/polarity/noise、calibrationStatus任务进度和两组统计，再复制交公共诊断快照。不要由核心0重新读ADC；网页10～20Hz不代替1ms运动记录。

ARM对象大小实测：Sample36字节，Estimate60，Calibration544，HallModule5252（含64条队列），HallPosition644，HallCalibration620。模块实例应为长期静态对象，避免塞进很小的核心1栈；校准导入和诊断复制也要算栈/RAM预算。

## 待公共层冻结的调整与影响（本窗口未修改公共接口文件）

| 所需公共调整 | 影响与集成动作 |
| --- | --- |
| ConfigTypes加入唯一Hall设置子集和全局校准域 | RuntimeConfig目前只有模块1。按本表范围冻结后将模块Settings改为别名/显式适配；公共层同时修改校验、不可变快照/两核ACK，模块4读回/模板字段同步 |
| InputTypes/MotionRecord明确滑轨数据 | mm、物理p、配置output、valid/quality/active、sequence/timestamp/revision/epoch；明确p反转约定和校准更新的版本/世代，不让USB二次倍率/反转 |
| 公共运动/诊断通道与恢复编排 | 采用现有SPSC或统一MotionQueue适配；诊断复制而非跨核引用；统一pause ACK/消费者清理/resume，溢出按当前位置恢复 |
| 控制/校准命令路由 | 将start/reference/node/beginSweep/finish/cancel/status转核心1，保留job_id、MapCheck阶段和成功采用边界，操作ACK不能先于两核实际应用 |
| 全局保存/导入schema及CRC | 只公共ConfigStore持有Flash，逐字段序列化Calibration，保存完成校准及全部模板一致快照；上电load失败禁用滑轨并报原因 |
| 唯一ADC所有权和调度预算 | 禁止analogRead、温度采样/另一核争用ADC；共享BoardConfig不改GPIO；实时服务需测，不能仅声明频率 |
| 普通绝对X、IO4及来源路由 | 由唯一USB后端编码，不重置独立Y；HID/IO4数值范围及失磁/接管D10～D12需实板/游戏冻结；本模块不输出USB包 |

统一的公共协议/存储/USB仍由集成窗口拥有；本目录不是已冻结的公共协议。用户本轮要求仅修改所属模块，所以只将接口需求和影响记录在此及进度文件，未扩大公共实现授权。

## 已验证与接手顺序

严格ARM编译6单元（含真实SDK驱动）通过；11组原始C++ ARM/Unicorn2.1.3测试全部通过，4,615检查。独立Arduino链接通过：Pico6.1.0、TinyUSB3.7.7、Generic RP2040、4MiB/noFS、W25Q64JV /4、125MHz，Flash85,036字节/RAM22,116字节。测试体积不等于最终整机资源；日志只有第三方cdc_host.h缺省字段警告，模块无警告。未上传、未实板、未主机/游戏验证。

先读本文件和docs/progress/模块2.md，再运行tests/module2/run_tests.py、compile_smoke.py复现；固定快照在tests/module2/validation_2026-10-03.json。接着由集成窗口按上表接唯一公共接口；按tests/module2/HARDWARE_CHECKLIST.md采真实曲线和耗时，再验证普通绝对X/相对Y及IO4/游戏。没有真实三路数据前，不宣称重心/折线能达到无抖动或快反向目标。

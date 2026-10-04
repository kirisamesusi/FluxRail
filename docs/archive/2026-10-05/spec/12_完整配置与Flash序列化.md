# 完整配置及双槽序列化契约（schema 1）

此文件保留阶段8的schema1冻结字节格式；阶段9当前写入schema2，差异与迁移见[IO4软件契约](13_IO4软件契约.md)。下面“当前”属于阶段8历史口径。schema1金样与字节顺序不改。

2026-10-03，公共集成阶段8。唯一类型在common/DeviceConfig.h；模块2/3算法与网页不变。本契约冻结存储/模板边界，运动设置保存不代表运动硬件已接入。

- 四预置固定ID：1 SDVX、2 ONGEKI、3 IN FALSUS、4 MULTI K；初始活动4。预置不能删除，可改名/参数，可单独恢复。自定义最多8个，总容量12；ID从5单调递增，不复用已删除ID，耗尽拒绝创建。名称1..31个可打印ASCII（排除引号及反斜杠），便于有界协议；中文显示名称由上位机处理。
- 每模板包含十二绑定、扫描、灯/指示，以及Hall Settings、两旋钮完整Settings（enabled/axis/invert/gain/counts_per_turn_at_gain1、滤波/可信转速/活动及PushSettings）、来源接管设置。PUSH动作只在bindings。音击两旋钮默认关闭。默认灯、鼠标OR、路由策略为记录中的工程起点。没有为游戏编造键位。
- DeviceConfig保存全部模板、活动ID、next_id、整体revision、启动USB模式、全局Hall校准与校准generation。Hall Settings/Calibration复用模块值类型，公共层逐字段编码；不把C++对象padding/bool/enum/内存布局当Flash格式。机械校准不属于模板；切换/复制/恢复模板均保留。
- 当前只允许NORMAL_NKRO启动；IO4能力仍false，后续独立schema扩展或明确迁移。未校准Hall可以在模板中enabled，但采集接入后必须拒绝输出有效位置。
- 模板编辑/切换只RAM，任何成功更改递增revision（跳过0）；显式save_config保存已确认完整快照，维护释放键鼠→核心1暂停ACK→Flash写/读回验证→最新状态重建→Resume ACK。成功回复在恢复之后。CDC断开不取消已接受工作。保存错误保留dirty并恢复输入；保存读回不确定时查询状态/重启，不能盲重试。

## 二进制

所有整数little endian、bool只能0/1、enum固定序号；Hall float为IEEE754 binary32，旋钮double为binary64，按位复制仅单个标量，不写对象。无效校准编码为valid=0且不带候选字段；有效校准版本1，完整范围/曲线/覆盖检查。Decoder检查精确长度、ID唯一、活动存在、四预置齐全、所有参数及有限数；输出仅在整体通过后发布。细节顺序见config/DeviceCodec.cpp与冻结字段表。

4MiB板、no FS：slot A=[0x3FB000,0x3FD000)，B=[0x3FD000,0x3FF000)，每槽8192字节，各含两个独立4KiB擦除扇区；最后EEPROM=[0x3FF000,0x400000)不使用。正式构建把build.flash_length固定为4173824（0x3FB000），链接及运行双重检查固件末尾/EEPROM/FS分区；禁止直接IDE默认分区宣称保存可用。

每槽：256字节header页、payload（最多7680字节）、最后256字节commit页（offset7936）。header前32字节依次u32 magic=0x314C5246、schema=1、payload_length、generation、snapshot_revision、payload_crc32、header_crc32（前24字节）、reserved=0；余下FF。commit前20字节u32 magic=0x54494D43、generation、payload_crc32、header_crc32、crc32（前16字节）；余下FF。CRC32/ISO-HDLC，poly0xEDB88320、初末xor FFFFFFFF。

保存只擦非当前有效槽，写header/payload（FF补足页），先读回逐字节及语义校验，再最后写commit页，再完整读回CRC/语义验证。旧槽始终保留。启动完整校验两槽；选择新generation（32位半范围比较，回绕跳0）；相等/半范围冲突固定选A并报告槽状态。不认识schema、损坏、缺commit或非法配置不采用；一槽坏退回另一槽；均坏回默认，dirty=true，不自动擦写。

临时预览、切模板、恢复默认不写Flash。断电恢复模拟覆盖擦除中、逐页/commit中、损坏header/payload/commit、语义非法、回绕、读回失败；这些模拟不是实板掉电验证。

## schema 1精确payload顺序

下面全部按列出顺序，不使用对齐padding。enum：ActionType disabled=0/keyboard=1/mouse=2；LightMode static=0/breathing=1/color_cycle=2/rainbow_wave=3/press=4；axis X=0/Y=1；polarity byte为1或255（-1）。名称为32字节ASCII、NUL结束；正常写入NUL后的尾字节为0。RGB为连续r/g/b三个u8。

1. Device头：u32 schema/revision/active_id/next_id，u8 profile_count/startup_usb_mode（18字节）。
2. 恰好profile_count个Profile，每个329字节，依次：u32 id、name[32]；十二Binding各u8 type/usage/modifiers/button；u32 retrigger_gap_us/press_debounce_us/release_debounce_us；u8 lighting.enabled/mode/brightness_level/per_key；u32 lighting.period_ms；u8 lighting.reverse/indicator_enabled；global RGB、十键RGB、indicator RGB。
3. 每个Profile接Hall Settings：u8 enabled/invert；f32 center_gain/range_min/range_max；u32 quiet_tau_us/moving_tau_us；f32 fast_speed_mm_s/activity_mm；u32 activity_hold_us；f32 enter_signal/exit_signal/curve_tolerance；u32 max_gap_us（50字节）。
4. 每个Profile接两个Knob Settings，每个66字节：u8 enabled/axis/invert；f64 gain/counts_per_turn_at_gain1；u32 filter_tau_us/max_gap_us；f64 max_turns_per_second；u16 activity_ticks；u32 activity_hold_us；u8 push.enabled；u32 push.press_us/rotating_press_us/release_us/rearm_us/max_hold_us/max_sample_gap_us。
5. 每个Profile最后为Routing：u32 idle_us、u8 simultaneous_hall_first（5字节）。当前同轴旋钮先动者持有、滑轨与X旋钮按活动接管及Y独立是待实现的工程起点，没有实现运动路由。
6. 全部Profile之后：u32 calibration_generation、u8 calibration.valid。若false即payload结束。若true：u16 version；三通道各f32 baseline/baseline_noise/gain＋u8 polarity；f32 left_mm/right_mm；u8 node_count；恰好node_count个Node各f32 mm/response[3]/noise[3]（28字节）；u32 sweep_segments；u8 sweep_left/sweep_right；f32 noise_mm。

默认四模板未校准payload=1339字节，CRC32=3954875198；独立金样tests/integration/schema1_defaults.bin。满12模板＋17节点payload=4507字节，小于7680上限。未知schema不按现有格式猜解；后续字段变化必须另增schema与明确迁移，不能改以上顺序后仍称schema1。

当前Hall/Knob值对象分别复用现有模块头文件，由公共层完整校验；未改模块业务实现。模块2/3还未参与App采集/配置ACK，当前两核握手实际应用模块1投影，运动字段是持久化配置能力，不能宣称运动运行生效。开机读回完整全局校准；真正HallModule::load及采样复用属于下一阶段。

## 阶段9 schema2读取及写入

保持两槽/分区/commit/CRC规则，仅header和payload schema同时为2；旧槽两者同时为1仍可完整校验并迁移。每个329字节旧Profile尾追加12个io4_bindings u8，成为341字节；Device头、校准字段顺序/标量表示不变。schema2启动模式0普通/1IO4；schema1只接受0。未知schema及header/payload版本不符拒绝，回退完整有效槽。schema1只读迁移保留所有旧模板ID/名称/next_id/活动/设置/revision/全局校准，补齐默认功能表，不自动写Flash；下次显式save写schema2。阶段9默认payload1387字节、CRC32=894450391（历史值）；当前阶段10默认CRC1053904252，schema2_defaults.bin独立金样；12模板＋17节点4651字节，仍低于7680上限。模式保存使用完整已确认DeviceConfig，不另设EEPROM模式字节。


## 阶段10默认配色更新（格式不变）

阶段9schema2默认金样CRC894450391是历史值，保留于阶段9验证快照。本阶段仅修改新建/显式恢复预置的indicator RGB：青/橙/紫/绿（详见模块1），当前schema2_defaults.bin仍1387字节，CRC1053904252。四个颜色偏移为payload的157/498/839/1180，均3字节；其他字节、字段顺序、schema2和分区不变。已有保存/导入/schema1迁移保留原色，不自动写Flash。schema1金样1339字节/CRC3954875198保持原样。

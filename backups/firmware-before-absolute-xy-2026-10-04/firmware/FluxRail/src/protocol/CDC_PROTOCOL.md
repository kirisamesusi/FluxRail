# FluxRail CDC配置与存储协议 v1 / device schema 2

更新：2026-10-03（北京时间），阶段10模块1完善（保留阶段9 IO4契约）。正式App的hello scope=device_schema2，固件0.8.1-module1-software；get_config仍是活动模板的runtime_module1投影，兼容既有模块1设置。完整配置见[序列化契约](../../../../docs/spec/12_完整配置与Flash序列化.md)。IO4及迁移扩展见[软件契约](../../../../docs/spec/13_IO4软件契约.md)。模块4只启用hello所声明的能力，并按usb_mode区别当前输出人格；本轮未修改网页。

## 连接与有界传输

普通人格沿用唯一TinyUSB CDC＋两HID接口。默认IO4是无CDC的单HID；不能向该人格发送本协议。IO4带CDC的编译候选FLUXRAIL_IO4_CONFIG_CDC=1独立待验，当前正式构建为0。Web Serial打开CDC后置DTR=true，建议115200波特率；固件不用波特率计实时输入。使用ASCII JSON，每请求一行，以LF结束；可用CRLF，CR计入行长。请求与响应都必须匹配id。

每行最多4096字节（不含LF）；空行忽略。接收固定4096字节，响应固定4097字节，JSON最多384个token、嵌套深度12。当前字段名、op、type和mode都是未转义ASCII字面值；ID、计数、微秒、RGB须十进制非负整数；motion中的浮点字段支持有限JSON数字（指数绝对值最多38），输出四舍五入到六位小数。归档校准或需要逐位保真时使用完整二进制快照传输。字符串转义可作语法检查，但不当成另一种字段名或枚举拼写。布尔须true/false，不用0/1。

超长内容丢到下一个LF才报LINE_TOO_LONG（id=null），不执行截断前缀；解析或校验失败完全不申请配置。对象拒绝未知/重复字段，数组长度和物理编号严格校验。每核心0轮最多读取128字节，最多向CDC FIFO复制64字节；TX部分写入保留剩余数据，待响应完成前停止读下一请求。TinyUSB操作使用端口现有非阻塞mutex，不调用Serial的阻塞写入/等待。

应用期间暂停协议读入，App继续服务USB、配置握手、输入与灯。客户端应串行请求；一般请求建议2秒超时，set_config/restore_defaults用至少4秒。应用超过3秒回APPLY_TIMEOUT，结果可能仍在推进：读get_status/get_config后判断，不能盲重试。CDC关闭DTR、挂起或断连后清半行及待发响应；已接受的配置由App继续完成。重连丢弃客户端旧半行/请求，重新hello及读回。

## 请求与应答

请求恰好四字段：

~~~json
{"protocol_version":1,"id":1,"op":"hello","payload":{}}
~~~

id范围1..4294967295，protocol_version固定1。正常应答含protocol_version/id/ok/revision/applied_revision/saved_revision/dirty，以及result或error。无法取得有效id时为null。

revision是最后确认快照版本；恢复或首次握手未完成时applied_revision=null、config_ready=false。忙时get_config返回上次已确认快照；首次确认前的快照不承诺生效。启动时完成配置后revision=1；配置操作由设备增加revision，4294967295回绕到1，调用者不能指定新revision。

saved_revision为本次启动已加载/写入且读回通过的完整快照revision，无有效槽时null。dirty比较最后确认配置与saved_revision；RAM预览、模板操作或导入均不自动写Flash。显式save_config结束完整维护后才回复成功。启动恢复保存的全部模板/活动ID/全局校准；无有效槽恢复默认且不自动写入。flash_save取决于固定分区运行检查；profiles/global_calibration_storage/snapshot_transfer为true，max_profiles=12。io4/usb_cold_switch为true，io4_game_verified为false；hall/knobs/push_hardware/absolute_x/bootloader_command仍false；配置保存不代表采样、运动或校准任务已接入。

| op | payload | 完成与结果 |
| --- | --- | --- |
| hello | {} | 产品、板型、固件版本、真实serial、普通人格、能力和限制 |
| get_config | {} | scope/config_ready/config，最后确认的模块1完整配置 |
| get_status | {} | config_ready/applying/apply_failed/requested_revision/physical_mask及协议计数 |
| set_config | expected_revision＋完整config | 完整校验后异步申请；两核ACK、旧流释放/重建、Resume ACK完成才返回成功及配置 |
| restore_defaults | expected_revision | 仅模块1映射/扫描/灯/指示恢复工程默认；同样等待两核确认 |

未实现的校准任务、遥测运动、升级命令返回UNSUPPORTED_OPERATION。set_config不是patch，未提供全部字段即拒绝。所有配置操作先要求config_ready且不busy，再比较expected_revision。

~~~json
{"protocol_version":1,"id":2,"op":"get_config","payload":{}}
{"protocol_version":1,"id":3,"op":"restore_defaults","payload":{"expected_revision":1}}
~~~

成功示例（省略的config必须以设备实际读回为准）：

~~~json
{"protocol_version":1,"id":3,"ok":true,"revision":2,"applied_revision":2,"saved_revision":null,"dirty":true,"result":{"scope":"runtime_module1","config_ready":true,"config":{}}}
~~~

错误示例：

~~~json
{"protocol_version":1,"id":4,"ok":false,"revision":2,"applied_revision":2,"saved_revision":null,"dirty":true,"error":{"code":"REVISION_CONFLICT","message":"Read current config before editing"}}
~~~

## config精确字段

| 路径 | 类型与范围 |
| --- | --- |
| keyboard.bindings | 恰好12项；每项source_id和action，按0..11顺序；0..9=BT1..10、10/11=P1/P2 |
| action.disabled | {type:"disabled"}，不得添加usage/modifiers/button |
| action.keyboard | {type:"keyboard",usage,modifiers}；usage=4..164或224..231，modifiers=0..255 |
| action.mouse_button | {type:"mouse_button",button}；button=1/2/4，分别左/右/中 |
| keyboard.retrigger_gap_us | 整数0..100000us |
| scan.press_debounce_us / release_debounce_us | 两字段都要，整数0..20000us |
| lighting.enabled / per_key / reverse | 三个独立布尔 |
| lighting.mode | static / breathing / color_cycle / rainbow_wave / press |
| lighting.brightness_level | 整数0..100；100对应最终实际最多50% |
| lighting.global_color | RGB数组，恰好三整数0..255 |
| lighting.key_colors | 恰好十个RGB数组，按BT1..10保存，自动配色也保留 |
| lighting.period_ms | 整数100..60000ms |
| indicator.enabled / color | 布尔及RGB数组；实际最终25%限幅，模式优先级仍待D06 |

config恰好keyboard/scan/lighting/indicator四对象。配置仍使用common/ConfigTypes唯一值对象，协议只负责序列化；热改键仍由公共ConfigCoordinator释放旧绑定并在新采样epoch重建。P1/P2虽可保存到模板绑定，但没有模块3过滤输入接入，不能显示PUSH硬件可用。

完整可解析请求见[cdc_examples.jsonl](../../../../tests/integration/cdc_examples.jsonl)。样例只作软件开发参考；本轮没有连接/操作设备。

## 错误与接入检查

INVALID_JSON（语法/ASCII/深度/token上限）、INVALID_REQUEST（信封/id类型）、PROTOCOL_MISMATCH、INVALID_PAYLOAD、INVALID_CONFIG、LINE_TOO_LONG、UNSUPPORTED_OPERATION、BUSY、REVISION_CONFLICT、APPLY_FAILED、APPLY_TIMEOUT、APPLY_SUPERSEDED、RESPONSE_TOO_LARGE。

BUSY/REVISION_CONFLICT先读状态或当前配置；APPLY_FAILED/APPLY_TIMEOUT/SUPERSEDED不代表回滚或没生效，要读回确认。SAVE_FAILED/SAVE_TIMEOUT可能已提交Flash，但未能确认读回或输入恢复；先查询状态，必要时重连/重启读取，不把错误当成一定未保存。超时后若旧请求仍占着CDC，等待其超时响应或重新连接再读回；不要将排队的旧页面写入当成最新配置。

验证见tests/integration/run_protocol.py：真实C++解析/编码/协议、InputWorker/ConfigCoordinator/ActionMapper，ARM Thumb指令执行；CDC仅替换FIFO/mutex边界。编译/模拟不代表Windows Web Serial、真实双核、USB主机/PIO或游戏通过。存储与实际App软件边界另外由run_storage.py/run_app_storage.py覆盖；后续运动扩展按能力声明接入。

## 完整设备与模板命令

所有写操作包含expected_revision，要求设备ready且不busy，成功后revision递增。读操作不改变revision。除了save_config，其余均为RAM操作。预置ID1..4固定且不能删除；自定义ID从5递增，最多8个；名称1..31可打印ASCII，排除引号/反斜杠。

| op | payload精确字段 | result / 语义 |
| --- | --- | --- |
| get_device | {} | schema/active_profile/profile_count/next_profile_id/startup_usb_mode/calibration_valid/calibration_generation |
| list_profiles | {} | active_profile/max_profiles及profile_id/name/preset列表 |
| get_profile | profile_id | profile_id/name/config（原模块1四对象）/motion/io4_bindings |
| set_profile | expected_revision/profile_id/config/motion | 替换config/motion，保留io4_bindings；config/motion不可省略，名称由rename改；确认后返回设备meta |
| create_profile或copy_profile | expected_revision/profile_id/name | profile_id指定复制来源，产生新的自定义ID；不自动激活，新ID由next_profile_id变化和list读回 |
| rename_profile | expected_revision/profile_id/name | 改名，保持ID |
| delete_profile | expected_revision/profile_id | 只删自定义；删除活动模板原子切回4 |
| set_active_profile | expected_revision/profile_id | 释放旧绑定、采样边界切换、恢复当前物理状态，确认后返回meta |
| restore_preset | expected_revision/profile_id | 只恢复1..4之一，保留全局校准及其他模板 |
| save_config | expected_revision | 保存全部一致快照；先释放键鼠、等待输入暂停ACK/USB释放传输完成、写入/读回、重建/Resume ACK，最终返回meta |

restore_defaults继续只恢复活动模板的模块1四对象，保留motion/名称/校准和其他模板；与restore_preset不同。所有模板改动等待当前模块1投影的两核确认；尚未接入的motion参数只有配置存储/读回能力。

motion恰好hall、knobs[2]、routing三个域。Hall字段顺序/范围沿用hall/README中的完整Settings。旋钮字段：enabled、axis（0=X/1=Y）、invert、gain（0.01..16）、counts_per_turn_at_gain1（1..1000000）、filter_tau_us（0..20000）、max_gap_us（500..100000）、max_turns_per_second（0.01..1000）、activity_ticks（1..8191）、activity_hold_us（0..1000000）、push。push恰好enabled、press_us（0..20000）、rotating_press_us（press_us..50000）、release_us（0..20000）、rearm_us（500..100000）、max_hold_us（1000..16000000）、max_sample_gap_us（500..100000）。动作只在config.keyboard.bindings[10/11]。routing恰好idle_us（0..100000）、simultaneous_hall_first（布尔）；运动路由尚未实现。

get_status额外提供maintenance=idle/pausing/writing/restoring/rebooting、save_result、valid_slots（bit0=A/bit1=B）。SaveResult数值：0=None、1=Ok、2=Unavailable、3=InvalidSnapshot、4=ReadFailed、5=EraseFailed、6=ProgramFailed、7=VerifyFailed。valid_slots是当前启动/写入已验证的槽状态；不承诺未重新读取的槽仍完好。保存期间同一CDC串行请求暂不读取下一请求；客户端发送save时立即显示维护状态。重连可查询未结束的App任务。保存协议超时10秒，客户端建议至少12秒；暂停/恢复ACK及等待USB释放有2秒超时，看门狗仍生效。写Flash关闭中断/硬件暂停另一核，是明确的维护阻塞阶段，不承诺实时USB/输入。

## 完整快照导入导出

单行限制保持4096；不把整个12模板快照塞进一行。导出按schema2逐字段二进制生成，使用lowercase hex，每块最多512字节；CRC32是无符号十进制整数。

| op | payload | 规则 |
| --- | --- | --- |
| export_config | offset | 返回schema=2/encoding=hex-le-schema2/length/crc32/offset/data/next_offset，最多512字节；首块offset=0，之后用next_offset直至length |
| begin_import | expected_revision/length/crc32 | 长度1..7680，隔离RAM候选，不改活动配置 |
| import_chunk | expected_revision/offset/data | 必须按序offset、非空偶数长度lowercase hex（最多1024字符），整个传输保持同一revision |
| commit_import | expected_revision | 长度/CRC/全schema/模板ID/校准完整检查通过后原子申请，设备生成新revision，两核确认后返回meta；不自动保存 |

导出各块必须来自同一revision，客户端检查每个应答revision/length/crc32，不符从offset0重读。export会取消尚未完成的import。CDC断开丢候选，不取消已接受的commit/save。修改配置后旧import不能继续/提交，需重新begin；导入提升next_profile_id至当前高水位，校准generation递增。导入兼容schema1并按只读迁移规则补齐IO4表；只允许其普通启动值。未知schema、校准NaN/畸形曲线/缺覆盖、悬空活动ID、预置缺失等拒绝，原配置保留。新增错误INVALID_PROFILE、INVALID_TRANSFER、STORAGE_UNAVAILABLE、SAVE_FAILED、SAVE_TIMEOUT。

可复用主机样例：[cdc_client.py](../../../../tests/integration/cdc_client.py)；只通过用户提供的连接收发，不自行发现/打开端口或上传。

## USB模式、IO4绑定与显式冷切换

当前实际模式本次启动固定。set_startup_usb_mode只改RAM完整快照，不热换、不擦写；save_config须单独发送且维护完全成功，重启命令不隐式保存。设置模式与选模板相互独立。所有写操作严格expected_revision，bindings/mode操作同样等既有两核维护握手确认。

| op | payload精确字段 | 结果与规则 |
| --- | --- | --- |
| get_usb_mode | {} | current/requested_startup/saved_startup/forced_recovery/reboot_ready |
| set_startup_usb_mode | expected_revision/mode | mode仅NORMAL_NKRO或IO4；确认后返回设备meta，新revision |
| set_io4_bindings | expected_revision/profile_id/bindings | 恰好12整数，source0..11顺序、功能0..12；独立于keyboard.bindings，新revision |
| reboot_to_saved_mode | expected_revision/target | target仅NORMAL_NKRO或IO4；完整当前revision须等于saved_revision，目标等于保存读回模式，维护无失败 |

get_usb_mode的current/requested_startup是字符串，saved_startup无有效槽时null，否则同样是字符串；forced_recovery和reboot_ready为布尔。reboot_ready只表示维护已暂停/释放并达到应答门槛，不表示重启已完成。get_device.startup_usb_mode也返回模式字符串。hello.nkro/mouse_buttons描述固件支持的普通输出能力，实际端点由usb_mode决定；io4=true表示软件后端存在，io4_game_verified=false明确未通过游戏。纯IO4不提供hello。

功能序号：0禁用、1/2/3左A/B/C、4/5/6右A/B/C、7/8左右Menu、9/10左右Side、11Test、12Service。get_profile.io4_bindings读回该表；旧set_profile保留它，复制模板携带它，restore_preset重新填工程默认[8,6,5,4,7,3,2,1,9,10,0,0]。该布局尚非用户确认的音击实物布局。restore_defaults只恢复普通模块1域，保留IO4表、motion和校准。

冷切换：暂停输入→清轴/主机灯→全释放并等待实际HID完成→成功应答完整复制至FIFO→私有非阻塞查询确认FIFO空且CDC端点总线完成→在USB mutex内确认空闲并detach→250ms间隔→应用重启。FIFO复制完成不等于总线完成。响应result沿用get_usb_mode字段；current仍是旧身份。MODE_NOT_SAVED拒绝未保存或目标不符；REBOOT_REJECTED拒绝维护/保存失败；REBOOT_FAILED/REBOOT_TIMEOUT禁止继续切换。客户端建议至少4秒等待应答，断线后重新枚举/hello/get_usb_mode才确认实际身份。丢失应答不能盲重发重启；读回或使用启动恢复入口。普通apply/save在CDC断开后继续既有任务，reboot应答未送达或DTR/USB断开则取消切换并恢复输入。

默认纯IO4返回普通路径：按住BT10（source9/GP9工程恢复键）再复位/上电，首次USB连接前强制普通应用，保存的IO4模式/模板/校准不变；经普通CDC设置NORMAL_NKRO→显式save→reboot。恢复键不在游戏中判长按组合。BOOTSEL仍是ROM UF2最后入口，不是本协议升级命令。BT10可由BoardConfig参数调整；键电平/首次枚举/250ms间隔尚待实板。

连续请求参考[io4_examples.jsonl](../../../../tests/integration/io4_examples.jsonl)与[cdc_client.py](../../../../tests/integration/cdc_client.py)。样例revision仅适用于该演示序列，真实使用每步取应答revision；返回普通使用mode/target=NORMAL_NKRO。主机样例不打开端口、不上传、不自动save。

## 0.9.0整合接入（CDC v1、device_schema2不变）

完整运行契约为docs/spec/14_模块整合运行契约.md。hello的motion_telemetry/hall_calibration/hall/knobs/push_hardware/absolute_x/bootloader_command表示编译接入；usb_host_verified和io4_game_verified明确false。ADC/SSI实际begin结果在get_motion内，不把能力当硬件验收。所有op沿原有请求id/协议/revision/成功或错误封套，4096/384/12及RX/TX预算不变，无主动事件。

| op | payload / 实际语义 |
| --- | --- |
| get_motion | 空；核心1值复制快照，sample_revision/epoch/timestamp、三ADC、Hall状态/输出、双旋钮累计圈数/PUSH/错误及校准状态；sample_ready=false不是成功采样 |
| get_calibration | 空；相同校准状态子集及running/adopting/global valid/generation |
| calibration_start | expected_revision、job_id（非零）、magnet_removed=true、left_mm、right_mm（人工实测）；先统一暂停ACK/释放，然后核心1任务ACK后恢复其他来源 |
| calibration_reference | expected_revision、job_id、channel=0..2，等核心1实际命令接受 |
| calibration_node | expected_revision、job_id、mm（实测标签），等核心1接受 |
| calibration_begin_sweep | expected_revision、job_id，合作式MapCheck后全轨扫 |
| calibration_finish | expected_revision、job_id；候选实际Hall采样采用后递增RAM revision/generation，再等完整配置ACK，非Flash保存 |
| calibration_cancel | expected_revision、job_id；取消保留旧对象 |
| enter_bootloader | expected_revision；沿维护释放及CDC应答总线完成门槛，detach至少250ms后进ROM，不上传UF2 |

校准进行时拒绝普通配置写入/save/冷重启/UF2，保持状态和校准命令可用。人工标签阶段可多次轮询；响应ACK说明命令被接受，采样完成须检查阶段与collected/required，不是按钮点击即完成。参考/节点错误、失败、取消不得覆盖旧全局记录。timeout/断连不盲重试已经接受的写命令；重连读回结果。软件协议/ARM边界测试已通过，Windows串口/主机/游戏未验证。

## 0.10.0 固件识别元数据（2026-10-04）

当前调度候选为0.10.1-hall，下方0.10.0节保留首次识别格式历史。

get_motion.hall新增只读诊断：adc_attempts（实际向量尝试数）、skipped_periods（固定1ms相位漏掉的整周期）、max_lateness_us（相对首个待执行deadline的最大迟到）、max_interval_us（相邻实际尝试起点的最大间隔）、max_span_us（最大向量跨度）、max_service_us（最大采集服务时间）。均uint32，回绕按无符号差分；包含维护停顿。用两份新鲜快照的adc_attempts差/设备timestamp差计算实际频率，主机轮询次数不是采样率。旧网页忽略新增读字段，CDC v1/schema2保持不变。

此预发布版只修复Hall/核心1调度，普通绝对X描述符仍有已知Windows Code10，usb_host_verified不变；不能宣称两项修复完成。兼容路线和实板/游戏边界见 [修复进度](../../../../docs/progress/修复.md)。

CDC v1 未改变。hello 的 firmware_version 从 FirmwareInfo.h 读取，新增 firmware_id="FLUXRAIL-FW-V1"、firmware_board="fluxrail-rp2040-4mib"、firmware_metadata_format=1。UF2 包含同一88字节识别记录，网页本地校验身份/板型并比较版本；旧 hello 仍兼容。字段格式见 [固件版本与UF2识别](../../../../docs/spec/15_固件版本与UF2识别.md)。

# FluxRail 运行、构建与发布

更新：2026-10-05。唯一正式入口`firmware/FluxRail/FluxRail.ino`，当前版本0.10.2。已验证与未测范围见[测试状态](docs/测试状态与待验收.md)，开发先读[交接入口](docs/FluxRail_开发交接_窗口必读.md)。本页提供现有操作流程，文档整理本身未执行功能测试或硬件操作。

## 软件与工具链

根目录PowerShell：

```powershell
cd C:/Users/admin/Documents/FluxRail
python tests/integration/run_all.py
```

执行模块1/2/3、公共集成/transport/protocol/storage/app_storage/io4/motion、九套Web测试（含组合保存与灵敏度兼容）、正式Arduino编译及正式ELF描述符检查。失败立即停，结果`tests/integration/build/full_results.json`，日志`build/full/`。仅软件回归可加`--skip-build`，结果不含新正式构建。IO4 timeout专门回归为`python tests/integration/run_io4_timeout_regression.py`；统一run_all目前未单列该专门入口，涉及超时改动应补跑。

固定依赖：Python3.12、Unicorn2.1.3、Node>=20、本机Arduino CLI0.35.3、Arduino-Pico6.1.0、ARM GCC16.1.0、TinyUSB3.7.7项目私有适配。Python依赖准备见[模块1测试说明](tests/module1/README.md)；Web无npm install或打包步骤。

正式构建FQBN：`rp2040:rp2040:generic:flash=4194304_0,boot2=boot2_w25q64jv_4_padded_checksum,freq=125,usbstack=tinyusb`；程序最大4173824B，`FLUXRAIL_STORAGE_PARTITION=1`、`FLUXRAIL_IO4_CONFIG_CDC=0`，详见[硬件分区](docs/硬件与引脚.md)。私有适配在生成前验证上游SHA，不改系统库。工具路径不同可直接向编译入口传`--cli`；其他测试工具的`--core/--tools`支持见各README，不默认更新依赖。

仅正式编译和描述符：

```powershell
python tests/integration/compile_firmware.py
python tests/integration/run_usb_descriptors.py
```

**每次正式编译成功，交付文件名必须带固件版本号。** 当前入口在UF2身份和源/链接审计通过后，输出`tests/integration/build/releases/<版本>/<UTC导出时间>/FluxRail-<版本>-<UTC导出时间>.uf2`及同名`.elf`；编译目录存在`.bin/.map`时也按同样规则导出。附同名`-manifest.json`（版本、导出时间、文件SHA256及大小）。同一版本反复编译按独立时间目录保留，失败构建不发布成功交付文件；失败复制可能留下`.pending`暂存目录，不作为可用固件。控制台打印带版本文件路径，给用户复制/交接时优先选择这些文件。

Arduino内部仍生成`tests/integration/build/arduino/FluxRail.ino.uf2`及相邻`.elf`，供既有描述符检查/回滚入口使用；此固定路径会覆盖。源和链接审计`build/source_link_audit.json`、构建记录`build/firmware_results.json`（新增`versioned_artifacts`含实际路径/摘要）。版本来自验证后的真实UF2记录，不能只改文件名。独立Module1/2/3烟测固件不可作为整机UF2；`.o`等内部缓存不是交付固件。

## 编译前回滚与版本发布

每次正式编译先由`firmware_rollback.py`生成`backups/firmware-rollback/<旧版本>/<UTC时间>/`，保存旧UF2/ELF、按旧构建SHA核对的完整对应源码、下一源码快照、changes.patch、依赖和manifest。备份缺失/不匹配则停止；不要直接绕过入口覆盖旧产物。恢复按每包README核对，设备配置备份与固件回滚分开。

当前保留0.10.1包`20261004T171125762981Z`及0.10.2包`20261004T171126137829Z`，0.10.2归档UF2 SHA256 `92991fa7e70e591fa16857ac09cd513eb337e6b6f0ca9cbe2824292379d9b479`。该值是归档文件身份，实际最新用户刷入文件hash未直接观测。

新版本只改`firmware/FluxRail/src/common/FirmwareInfo.h`的`FLUXRAIL_FIRMWARE_VERSION`，然后按改动跑协议/模块/Web相关回归、正式编译及描述符检查。编译用Web同一解析器检查最终UF2身份/板型/版本；已有文件可用`node tests/integration/check_firmware_version.mjs`核对，不能替代重新构建。

识别格式：88B小端记录，magic`FLUXRAIL-FW-V1`，format1，board`fluxrail-rp2040-4mib`，version最长31ASCII，end_magic`0x46525746`；详见[原始格式冻结稿](docs/archive/2026-10-05/spec/15_固件版本与UF2识别.md)。格式变化同步固件、hello、Web解析器/控制器和fixtures。UF2进入ROM、手动复制后重连读版本；网页不会自动传UF2，识别头不作签名。

## Web运行与配置

```powershell
node web/server.mjs
```

打开`http://localhost:4174`，使用桌面Chrome/Edge选择串口。端口占用先确认现有服务；可用`FLUXRAIL_WEB_PORT`换端口。Web Serial端口通常独占，先在页面正常断开再使用测试客户端。只监听本机；模拟演示、内嵌浏览器UI检查不能代替真实连接。

编辑网页草稿→应用到设备RAM并等ACK/读回→需要时单独激活模板→显式保存Flash。预置保护、创建自定义、保存确认、IO4草稿写锁和revision冲突见[Web交接](docs/modules/05_Web配置工具.md)。真实旋钮显示来自累计圈数，本地预览不产生输入。

完整Web测试：

```powershell
cd C:/Users/admin/Documents/FluxRail/web
npm test
```

## 装配与校准验收流程

先核对正式4MiB目标板电源/接线/FPC脚序、UF2身份与保留区，再按用户实测安排普通USB→十键/灯→原始ADC/SSI/PUSH→校准和运动→保存复用→IO4→游戏验证。COM号和设备快照须重新读取。

无有效校准时Hall不输出，ADC诊断仍可读。移开磁铁采空场，输入实际左右毫米端点，依次三参考、7～17实际标签、映射检查与全轨扫、finish采用；失败/取消保留旧校准。成功仅RAM，显式保存后才能掉电复用。不得用约80mm估计或软件曲线替代装配实测。

普通标准绝对XY启动中点，Hall移动携带本设备旋钮Y；测试Hall扫动期间Y保持、运行时保存/切模板/重连保持、新启动归中，及ID1真实按钮/ID2中性按钮在Windows/游戏中的交互。双SSI真实帧/CRC/方向/PUSH和同时反向分别验收。

IO4编辑：普通CDC读回→改独立IO4功能表→应用→激活所需模板→应用启动IO4→再次显式保存→重启到保存身份。普通键盘表独立。纯IO4无CDC，按住BT10/GP9复位/上电强制普通，之后保存NORMAL才能改变下次启动。BOOTSEL是ROM恢复；[清空Flash工具](firmware/recovery/README.md)会清应用及全部模板/校准，现有工具说明仍为未实板执行。

现有实测脚本可能写RAM/Flash或换USB身份；先读[测试状态](docs/测试状态与待验收.md)、[裸板运行说明](tests/integration/BARE_BOARD_RUN.md)和脚本具体范围，建立新版本证据目录，不能把它们等同run_all的软件测试。完整硬件清单链接在状态表，旧步骤里的“全部未执行”按日期理解。

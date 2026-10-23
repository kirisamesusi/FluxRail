# 模板名 UTF-8 支持：主机侧验证（2026-10-11）

范围：用户授权为“中文模板名”修改固件。固件源码版本已升到 0.10.3（`FirmwareInfo.h`，见[固件升级日志](../../../docs/固件升级日志.md)；未编译、未发布 UF2）。

## 改动
- `ProfileManager.cpp`：`validName` 由“1..31 可打印 ASCII”改为“合法 UTF-8（1～3 字节序列），总长 ≤ 31 字节；拒绝控制字符、引号、反斜杠、DEL、C1、零宽/双向控制与代理区、非字符”。旧 ASCII 名称全部仍合法。存储槽仍是原 32 字节名称字段，**不改 schema 和 Flash 布局**。
- `Json.h/.cpp`：新增 `Json::utf8Value`（解码 `\uXXXX` 与代理对为 UTF-8）与 `JsonWriter::name`（把 UTF-8 重新写成 `\uXXXX`，线路保持纯 ASCII）。
- `ConfigProtocolDevice.cpp`：create/rename 解码，两处名称输出走 `w.name`。
- Web：`config.js` 名称规则（1～10 个字符、UTF-8 ≤ 31 字节；读回旧长 ASCII 名仍接受）、`client.js` 请求行 `asciiJson`、`snapshot.js`/`demo.js` 的 UTF-8 编解码。

## 验证（分层）
| 层 | 结果 | 说明 |
|---|---|---|
| x86 主机测试（本目录 `utf8_name_tests.cpp`，直接编译真实固件源码） | 96 项 / 0 失败，见 `host_test_output.txt` | 覆盖 ASCII 回归、中文、emoji 代理对、超长、非法序列、过长编码、控制/零宽字符、往返 |
| Web Node 测试 | 81/81，见 `web_test_output.txt` | 含 `asciiJson` 往返与名称规则 |
| ARM 交叉编译 / UF2 | **未做** | 本环境无 ARM 工具链 |
| 实板（刷入后创建/改名中文模板，保存、断电重读） | **未做** | 需要用户正式编译并刷机 |

结论：协议与校验逻辑在主机上成立；**尚不能声称实板通过**。旧固件收到中文名会返回名称错误（Web 默认新建名仍是 ASCII `Custom N`，所以旧固件不受影响）。

## 文档检查
`python docs/tools/verify_docs.py --output tests/integration/utf8_name_2026-10-11/verify_docs_2026-10-11.json`：退出码0，missing_links=[]，archive_errors=[]，文档93份；未改写冻结的 `docs/archive/2026-10-05/DOCUMENTATION_CHECK.json`。

## 下一步（未完成）
用户正式ARM编译→刷机→实测中文模板创建/改名/保存/断电重读；先决定是否升固件版本。完整步骤见 [窗口必读“待办”](../../../docs/FluxRail_开发交接_窗口必读.md)。

# FluxRail 旧 Web 备份说明

版本：v1.0 · 更新：2026-10-04。本文明确 backups/ 中旧 Web 的身份、范围和恢复方式。

## 1. 当前版与备份版

**项目根目录的 web/ 是唯一当前运行与开发版本。下面各目录都是修改前的历史备份，不是当前版本，不在备份里继续开发。**

最初要求保留的“旧 Web”是 [web-before-layout-2026-10-04](web-before-layout-2026-10-04/BACKUP-README.md)，位于左右布局重构之前。其他快照是在后续修改前追加的不同恢复点，不是对最初旧 Web 的替换。

当前操作规则见 [Web 开发与调整交接](../docs/FluxRail_Web_开发与调整交接.md)。本文不执行恢复、覆盖或删除，只记录操作方法。

## 2. 现有快照目录

| 备份目录 | 备份前状态 / 对应修改 | 范围 | 原文件数 |
| --- | --- | --- | --- |
| [web-before-layout-2026-10-04](web-before-layout-2026-10-04/BACKUP-README.md) | 左右模块工作区重构前的原 Web；最初旧版 | 完整 web/ 快照 | 28 |
| [web-before-physical-layout-2026-10-04](web-before-physical-layout-2026-10-04/BACKUP-README.md) | 已有左右布局，尚未按 PCB 参考调整十二路摆位/迁移旋钮设备状态 | 完整 web/ 快照 | 35 |
| [web-before-template-management-2026-10-04](web-before-template-management-2026-10-04/BACKUP-README.md) | 已有物理布局与输入状态调整，尚未加入当前模板保存/预设恢复/二次确认流程 | 完整 web/ 快照 | 38 |
| [web-before-header-image-2026-10-04](web-before-header-image-2026-10-04/BACKUP-README.md) | 模板调整后、页头仍用原SVG，尚未换为fluxrail-lace-bunny | 局部文件：index.html、style.css、server.mjs | 3 |
| [web-before-production-refactor-2026-10-05](web-before-production-refactor-2026-10-05/BACKUP-README.md) | 固定模板迁入生产前，保留已有未提交修改 | 完整web/74文件和4份当前文档；manifest.json逐项SHA | 78 |
| [web-before-mascot-2026-10-05](web-before-mascot-2026-10-05/BACKUP-README.md) | 水滴标志和兔子插画，新增水色少女吉祥物与icon之前 | 完整web快照；复制时逐项SHA/大小校验 | 50 |

这些是 Web 源文件、测试、历史截图/说明的版本快照；不是固件、整个工程、设备 RAM/Flash 或用户模板数据备份。换图前快照缺少 src/、package.json 等，不能单独启动或替代整个 web/。

完整快照保留了当时的 README 和测试。其文字/测试数仅属于该版本；例如旧“修改模板”跳转键位、草稿锁、遥测位置不能用来覆盖当前需求。备份测试包含对项目 tests/integration/ 的相对引用，不能因在 backups/ 多了一层目录而直接在那里运行并宣称快照损坏；应放到项目根旁的暂存目录或正式恢复到 web/ 再验证。

## 3. 文件清单与校验

每个目录的 BACKUP-MANIFEST.json 记录原快照文件的相对路径、字节数和 SHA-256。本次为换图前的三个文件补充清单；原三份清单及其记录的原文件保持不变。

本次逐项核验：原三份完整备份 28+35+38＝101 个文件均与原清单的大小和 SHA-256 一致；局部快照3个文件按新增清单核验。每个目录另加的 BACKUP-README.md 是本次归档说明，不计入原快照文件数，也不复制进恢复的 Web。

只读校验示例，PowerShell 在项目根目录执行，按需替换备份名：

~~~powershell
cd C:/Users/admin/Documents/FluxRail
$taskBackupRoot = (Resolve-Path 'backups/web-before-layout-2026-10-04' -ErrorAction Stop).Path
$taskBackupPrefix = $taskBackupRoot.TrimEnd('\') + '\'
$taskManifest = Get-Content -LiteralPath (Join-Path $taskBackupRoot 'BACKUP-MANIFEST.json') -Raw -Encoding UTF8 -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop
if (-not $taskManifest.files -or $taskManifest.files.Count -eq 0) { throw '备份清单为空' }
foreach ($taskEntry in $taskManifest.files) {
    $taskTarget = [System.IO.Path]::GetFullPath((Join-Path $taskBackupRoot $taskEntry.path))
    if (-not $taskTarget.StartsWith($taskBackupPrefix, [System.StringComparison]::OrdinalIgnoreCase)) { throw '清单路径超出备份目录' }
    $taskFile = Get-Item -LiteralPath $taskTarget -ErrorAction Stop
    $taskHash = (Get-FileHash -LiteralPath $taskTarget -Algorithm SHA256 -ErrorAction Stop).Hash.ToLowerInvariant()
    if ($taskFile.Length -ne $taskEntry.bytes -or $taskHash -ne $taskEntry.sha256) { throw "备份不一致：$($taskEntry.path)" }
}
Write-Output ('Verified backup files: ' + $taskManifest.files.Count)
~~~

旧清单中restore字段的简略复制说明保留为历史元数据，当前恢复统一按本文件第5/6节和files清单执行，排除后加说明。清单校验失败时先核对文件，不直接覆盖当前版本。清单只描述 Web 快照；不要把它误当固件校验或设备配置导出。

## 4. 后续修改前怎样新增备份

1. 先确认唯一当前目录为项目根 web/，记录这次要修改的内容。
2. 在 backups/ 新建有目的、日期及必要时间的目录，避免同日覆盖已有快照。
3. 页面结构、模板或设备交互修改默认复制整个 web/；只备份若干文件时明确标为局部并逐个列出。保留当前 assets/、src/、package.json、服务器和相应测试/说明。
4. 为复制的文件生成路径、字节数、SHA-256 清单，并与复制前文件逐项核对。
5. 写 BACKUP-README.md 标明“旧 Web 备份”、来源、范围、用途、是否完整、恢复步骤及未包含的设备/固件数据；说明与清单均不算原快照文件。
6. 更新本目录表和当前 Web 交接中的备份/验证记录。新开发继续在 web/，不要修改旧清单或原快照。

图片更换若引用根目录素材，应明确根素材与 Web 资源副本的关系；后续完整 Web 备份包含资源副本。当前原素材是 ../fluxrail-lace-bunny.png，当前运行资源位于 ../web/assets/fluxrail-lace-bunny.png。

2026-10-05后续视觉修改保留上述旧素材，当前右侧改用 `web/assets/fluxrail-mascot-controller.png`。水色角色原图、头像、右侧插画及主题配色原图均位于 `web/assets/`；来源、提示词、离线缩放导出和HTTP范围见[资源说明](../web/assets/MASCOT.md)。新完整备份须包含这些资源，不从本机临时目录或Codex生成目录恢复。

## 5. 恢复完整 Web 的步骤

1. 选定具体版本，先读该目录的 BACKUP-README.md，确认是完整快照；要回到最初旧版则选择 before-layout。
2. 结束键盘监听，核对网页草稿/设备未保存状态。恢复文件不会自动保存或回滚设备；关闭本项目预览服务，通常在其终端 Ctrl+C，不结束其他 Node 服务。
3. **先把当前 web/ 另存为新的完整恢复前备份并校验。** 保留现有全部历史快照。
4. 创建项目根目录旁的空暂存目录，例如 web-restore-staging-日期时间。仅按所选清单 files 列出的路径复制原文件，保持相对结构；不复制 BACKUP-MANIFEST.json、BACKUP-README.md 等归档说明。
5. 核验暂存目录内容与该清单一致，在暂存目录执行其当时 package.json 的测试命令；可用独立端口运行暂存版预览。不要要求旧版通过当前新增模板测试，也不要在备份原目录开发。
6. 核对绝对路径都在 FluxRail 项目内，把现有 web/ 保留到恢复前备份/保留目录，再将暂存目录置为 web/。使用一套 PowerShell 原生命令和 LiteralPath 操作；不将计算出的路径交给另一个 shell 删除或移动。
7. 不直接“覆盖复制后即算恢复”：覆盖会遗留 layout.js、template-actions.js、新测试或资源，形成不同版本混用。不要直接递归删除未核对的目录，也不要自动清理备份。
8. 按恢复版 server.mjs 重新启动并刷新，检查入口、资产加载、模板/草稿锁，记录所选快照、校验和实际结果，更新当前交接文档。

历史服务器可能不支持 PNG 或新资源路径。完整回退应使用该快照的 HTML/CSS/JS/服务器一致组合；当前根目录素材不必删除。上述流程只恢复网页文件，不刷写固件、不清理设备模板或全局校准。

## 6. 恢复局部换图快照

先完整备份当前 web/ 并校验，再停止本项目服务器，只把清单中的 index.html、style.css、server.mjs 恢复到对应位置。重新检查这些文件与当前 src/、控件 ID、样式和资源是否兼容，再启动和浏览器验证。

该快照形成于当前模板逻辑之后，恢复它会回到原页头 SVG 及旧静态资源白名单，不回退模板 JS。不需要删除根图片；web/assets/ 中图片即使保留，也不代表仍会被旧服务器公开。不要把3文件局部快照当成整套旧 Web。

2026-10-06本轮[灵敏度修改前快照](web-before-sensitivity-2026-10-06/BACKUP-README.md)：局部源码/资源与现行文档87文件、逐项SHA校验，包含先前未提交生产重构成果及两模块测试源码；未复制历史测试证据/工具，不是完整Web恢复包或设备配置。firmware-protection.json只用于核查93个固件文件未变。

2026-10-06[一位小数输入限制前快照](web-before-decimal-input-2026-10-06/BACKUP-README.md)：7文件局部备份，保留已有未提交修改并逐项核对SHA，非完整Web或设备恢复包。

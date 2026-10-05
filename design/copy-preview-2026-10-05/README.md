# FluxRail 文案迭代预览与历史记录

当前最终方案已保存为 [固定重构模板](../web-template-2026-10-05/README.md)，新窗口从 [正式交接](../../docs/Web重构交接_2026-10-05.md) 开始。本目录4175仍可查看迭代页；下面各轮小节是历史记录，以固定模板与最终交接为准，不按早期方案重建。

2026-10-05。用户已选定当前版本作为下一窗口重构模板，生产页面尚未接入或合并。

文案审查由 GPT-6-Luna（`gpt-6-luna`）承担。`COPY_REVIEW.md` 记录编辑判断，`copy-replacements.json` 保留原文和改文，`evidence/applied-copy.json` 记录哪些文案在预览页面中出现。没有在页面中出现的条目属于其他运行状态或提示，供后续合并时另行核对。

## 打开预览

从仓库根目录运行：

```powershell
node design/copy-preview-2026-10-05/server.mjs
```

访问 http://localhost:4175 。也可直接打开本目录的 `index.html`。页面可切换八个模块、展开进阶分组，倍率控件和重启确认弹窗可本地试用；设备操作均未接入。

## 这一版的范围

`index.html` 是当前 Web 布局与表单的独立副本；动态表单通过项目已有的隔离模拟演示只读抓取。所有设备状态均为等待提示，表单值仅用于呈现布局。没有接入设备协议、串口、配置保存、固件更新或输入测试。`shell.js` 处理页面导航、展开、倍率输入同步和本地重启确认。`style.css` 与复制时的原版相同，角色图片也沿用原版，新增外观调整全部放在 `preview.css`。

现有 `web/`及固件未因本次预览修改；交接阶段更新文档导航并另建固定模板。未编译、未操作硬件、未合并或提交。

## 后续修改与交接

本窗口后续继续修改这份预览，用户满意后再交给另一个窗口处理生产页合并。不要直接替换生产 `index.html`：这份 HTML 冻结了表单并停用了控件，只能作为文案和排版参考。

合并时按 `copy-replacements.json` 将文案分别放回 `web/index.html` 和相应 `web/src/` 生成处，保留原有控件 ID、事件、数据、协议及保存逻辑。模板弹窗、设备状态和错误提示需要在实际相应状态下另行检查。

`tools/` 是本次复制、应用文案和视觉检查工具；`evidence/` 保存开始时 Web 文件哈希及本次浏览器检查，不构成设备或游戏验收。

## 预览调整：应用并保存

用户提出将主区“应用设置”和“保存到设备”合成“应用并保存”。预览已合并按钮，并同步普通操作提示、侧栏步骤、页脚和模板说明；调整记录在 `preview-edits.json`，本次独立检查在 `evidence/combined-save/`，第一稿证据保留。

这只是空壳中的交互提案，尚未实施生产逻辑。后续合并需复用现有应用及保存流程：先校验、应用并确认设备已采用，再保存完整设备配置并读回确认；没有页面修改时也应允许保存设备上已应用但未保存的设置。应用失败不继续保存；应用成功但保存失败时应提示“设置已应用，但未保存”，结果未知时先重新读回。模板选择和启用仍是不同动作，不因应用并保存而自动启用其他模板。IO4、校准、启动模式等专用流程仍须按各自确认及状态锁处理。

## 21条浏览器批注修订

模式选择移到模板管理，IO4恢复说明独立，重启在顶部并提供本地确认；滑轨校准并入滑轨高级参数，重复下载说明归入固件更新，IO4摆位与普通映射一致。三个倍率控件支持本地刻度尺与小数输入，初始显示5、范围0.1～10。此范围是预览方案，现固件滑轨上限仍为8，正式修改需同步校验。

全部操作及交接事项见根目录 [web修改.md](../../web修改.md)。本轮评审在 [Luna评审](LUNA_REVIEW_批注版.md)，浏览器证据在 `evidence/annotations-21/`。前一版证据保留。重建批注版用 `tools/apply-copy.mjs`，检查用 `tools/check-annotations.mjs`，均需传入已安装Playwright的 `index.mjs` 路径；旧 `check-preview.mjs` 是上一版检查工具，不适用于已启用本地控件的新预览。

## 第二轮4条批注

最新页面仍通过 `tools/apply-copy.mjs` 重建，检查脚本为 `tools/check-round2.mjs`，证据保存于 `evidence/annotations-round2/`；旧检查脚本仅对应旧版本。上轮副本在 `evidence/annotations-21/preview-before-round2/`，本轮记录不覆盖旧截图及评审。

模式开关、确认弹窗和倍率尺均只在页面本地演示，关闭或刷新页面不持久保存，不接触实际设备。倍率1～10仍属于用户设计方案；当前设备滑轨允许0.1～8，生产合并必须同步校验，读回旧倍率时不自动改写设备配置。

本轮四条批注已完成，GPT-6-Luna独立评审见 [第二轮评审](LUNA_REVIEW_第二轮.md)。最后的固件更新文案精简由 `tools/check-update-copy.mjs` 单独核对，结果和最终截图在 `evidence/annotations-round2/update-copy-final.json` 及同目录图片；74个生产Web文件哈希仍与复制前一致。

## 第三轮12条批注

最新导航顺序为设备状态、按键映射、灯光设置、滑轨设置、旋钮设置、模板管理、固件更新、实验性功能。设备信息和原输入测试合并至设备状态；三个输入概况卡片和模板概况可跳转设置。设备值保持等待读取，页面编辑模板明确标为预设示意，不代表已连接设备。模拟演示只保留在实验区临时测试分组，仍未接入。

IO4警示合成统一说明，开关使用与整页一致的浅蓝白玻璃样式；两处侧栏标题加大。模板容量及提示精简，按键使用“个”计数，物理下载提示只保留BOOT接USB方法，滑轨高级标题同时注明校准。

重建入口仍为 `tools/apply-copy.mjs`，本轮新增转换在 `tools/apply-round3.mjs`，检查入口为 `tools/check-round3.mjs`。证据保存于 `evidence/annotations-round3/`；旧工具仍对应旧版本，第二轮实现副本在 `evidence/annotations-round2/preview-before-round3/`。

[第三轮Luna评审](LUNA_REVIEW_第三轮.md)已完成，未发现空壳审阅阻断项。模板开头按评审建议再次精简，其最终核对和截图保存在 `evidence/annotations-round3/profiles-copy-final.json` 及同名图片。全部操作记录见根目录 [web修改.md](../../web修改.md)。

## 第四轮两处微调

IO4选中恢复浅粉玻璃态，顶部常驻卡片改为“设置与保存”并精简说明。按钮与交互未变；专项检查入口 `tools/check-round4.mjs`，证据在 `evidence/annotations-round4/`。原第三轮副本保存在 `evidence/annotations-round3/preview-before-round4/`。[第四轮Luna评审](LUNA_REVIEW_第四轮.md)已核对通过。

## 顶部区域合并

最新版本将页头、操作提示和保存工具栏合成一个“设备配置”顶部卡片。原按钮、动态反馈和状态节点保留，设备功能仍未接入。转换在 `tools/merge-header.mjs`，检查在 `tools/check-header-merge.mjs`，截图及报告在 `evidence/header-merge/`。此前版本保存在 `evidence/annotations-round4/preview-before-header-merge/`；旧检查脚本与报告只对应各自版本。

[顶部合并Luna评审](LUNA_REVIEW_顶部合并.md)已完成，未发现阻断项。

最新微调已去掉顶部合并区域的边框、圆角、底色和阴影，标题、操作和状态内容保留。去框前副本在 `evidence/header-merge/preview-before-unframed/`，最新视觉证据在 `evidence/header-unframed/`；仍仅修改独立预览样式。

[顶部去框Luna复核](LUNA_REVIEW_顶部去框.md)已完成，未发现阻断项。

最新批注已移除按键、滑轨、旋钮和模板管理标题旁的标签，其他内容保留。独立证据在 `evidence/heading-badges-removed/`，[标题标签Luna复核](LUNA_REVIEW_标题标签.md)已完成；原Web未修改。

## UF2确认弹窗

最新固件页已移除三张步骤卡，原“进入 UF2 下载模式”按钮在空壳中可打开本地二次确认；操作步骤与物理进入备选方法统一放到弹窗。取消、Escape和确认均不操作设备，确认只更新预览提示。文件检查入口保持原样。

转换工具为 `tools/uf2-confirmation.mjs`，最新交互检查为 `tools/check-uf2-dialog.mjs`，证据在 `evidence/uf2-confirmation/`；改前副本在 `evidence/heading-badges-removed/preview-before-uf2-dialog/`。旧检查脚本只对应旧版本。

[UF2确认流程Luna评审](LUNA_REVIEW_UF2确认.md)已完成，未发现阻断项；设备功能仍未接入。

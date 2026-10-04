# 2026-10-05 旧文档归档

这是整理前规格、阶段进度、交接与审查的历史档案。**当前开发先读[文档中心](../../README.md)与[新窗口必读](../../FluxRail_开发交接_窗口必读.md)**，不要把原稿中“当前”“未接入”“未实板”当今天的事实。

原稿保留历史状态与失败记录，仅重定位Markdown本地链接。原始字节保存在[整理前ZIP](../../../backups/docs-before-reorganization-2026-10-05/original-documents.zip)，原SHA/归档SHA及路径见[ARCHIVE_MANIFEST.json](ARCHIVE_MANIFEST.json)。正文纯文本路径按原项目根解释；第三方/旧板提及已删除产物由对应参考README说明。

## 按模块查原稿

| 类别 | 规格和设计 | 原交接/进度 |
| --- | --- | --- |
| 按键与灯 | [02模块1](spec/02_模块1_键盘与灯光.md) | [模块1](progress/模块1.md) |
| USB/IO4 | [09兼容冷切换](spec/09_IO4兼容与USB冷切换.md)、[10外部对比](spec/10_IO4外部项目对比.md)、[13软件契约](spec/13_IO4软件契约.md) | [IO4](progress/IO4.md)、[专项交接](handoffs/FluxRail_IO4专项_开发交接.md) |
| 滑轨 | [03模块2](spec/03_模块2_三霍尔滑轨.md) | [模块2](progress/模块2.md) |
| 旋钮/PUSH | [04模块3](spec/04_模块3_MT6701旋钮.md) | [模块3](progress/模块3.md) |
| Web | [05模块4](spec/05_模块4_Web上位机.md) | [模块4](progress/模块4.md)、[Web交接](handoffs/FluxRail_Web_开发与调整交接.md)、[旧Web README](source-readmes/web/README.md) |
| 公共与存储 | [01公共接口](spec/01_公共接口与集成约定.md)、[11工程组织](spec/11_Arduino模块拆分与四窗口交接.md)、[12字节序列化](spec/12_完整配置与Flash序列化.md)、[14运行契约](spec/14_模块整合运行契约.md) | [整合](progress/模块整合.md)、[旧固件README](source-readmes/firmware/FluxRail/README.md) |
| 发布与识别 | [15版本/UF2](spec/15_固件版本与UF2识别.md) | [旧运行说明](root/RUN_INTEGRATED.md)、[修复](progress/修复.md) |
| 需求与硬件 | [00需求索引](spec/00_项目需求与决策.md)、[07旧项目审查](spec/07_旧项目审查与来源.md)、[08决策清单](spec/08_功能缺口与使用场景清单.md) | [主需求](handoffs/FluxRail_需求总览_用户版.md)、[硬件提取](handoffs/rp2040_kb_firmware_notes.md)、[旧统一交接](handoffs/FluxRail_开发交接_窗口必读.md) |
| 验收与审查 | [06验收计划](spec/06_验收与实测计划.md) | [测试完整流水账](progress/测试.md)、[10/04总审查](root/REVIEW_FULL_2026-10-04.md)、[旧根README](root/README.md) |

原始测试/审核/JSON/日志仍在项目`tests/`，Web评审仍在`web/tests/`；现有备份与第三方源码保持原位置。旧路径保留短兼容入口，可继续从旧链接打开原稿。

## 本轮整理检查

- [DOCUMENTATION_CHECK.json](DOCUMENTATION_CHECK.json)：本地Markdown链接、归档映射、原ZIP与保护清单一致性。
- [ARTIFACT_NAMING_CHECK.json](ARTIFACT_NAMING_CHECK.json)：用户补充的版本文件命名规则及构建导出函数检查，未编译或上传固件。
- [文档维护与归档](../../文档维护与归档.md)：现行文档如何维护、主要冲突结论。

34份主文档进入此档案；其余源码旁技术说明、模块测试说明、第三方参考及备份副本保留就地。阅读按主代理及Luna模块代理分工完成，备份副本按来源比较，不把副本累计为新的需求。

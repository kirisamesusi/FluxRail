# FluxRail 旧RP2040-Zero参考

本目录从旧arduino-rp2040-zero-1-8-rp2040整理保留，仅供参考。源代码未改，新板GPIO/三霍尔/NKRO重触发/USB身份不能直接照刷。历史名字ARC2/RP2040RhythmController保留来源，新项目产品名FluxRail。

保留：
- [原版源码](original-source.ino)，从重复dist包独立取出。
- [边缘优化版主程序](firmware/RP2040RhythmControllerEdgeCalibrated/RP2040RhythmControllerEdgeCalibrated.ino)、同目录BoardConfig.h/EdgeResponse.h。
- [霍尔诊断源码](firmware/HallMouseEdgeCalibratedTest/HallMouseEdgeCalibratedTest.ino)。
- [算法测试脚本](tools/test_edge_trial.py)，运行时须重新准备ARM构建/Unicorn/pyelftools；本轮未运行。
- [旧网页](web/index.html)、serve.py/start-web.cmd，仅作交互/通信参考。
- [边缘优化历史说明](EDGE_TRIAL_1.md)、[原README历史档](README_historical.md)、原.git历史。

已删除build缓存、dist重复副本/ZIP/旧板UF2、sketch内构建副本和旧发布打包器；旧失败IO4只保留核心样本到../io4/failed-lkick。历史文档中的旧刷写包和旧绝对路径不再可用，旧编译测试结果不等于FluxRail实板结果。

重复键核对：原版及优化版都把物理按住状态OR到同一usage位，第二个A按下不重触发。FluxRail最新需求不同，需新增A-up/down队列，详见[旧代码审查](../../docs/spec/07_旧项目审查与来源.md)。

清理依据与路径清单见[整理记录](../CLEANUP.json)；新需求见[FluxRail总览](../../docs/spec/00_项目需求与决策.md)。

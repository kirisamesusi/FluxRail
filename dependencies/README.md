> 2026-10-05现行USB交接见[USB与IO4](../docs/modules/02_USB与IO4.md)及[测试状态](../docs/测试状态与待验收.md)。下文依赖适配说明保留；早期“未实板”仅属于原记录，最终4MiB/游戏仍未验收。

# 固定项目TinyUSB适配

prepare_tinyusb.py读取已安装Arduino-Pico6.1.0所带Adafruit TinyUSB3.7.7，先核对Device.cpp/.h和cdc_device.c三份固定上游SHA256，再生成generated/Adafruit_TinyUSB_Arduino。不修改系统安装库，不下载新依赖。compile_firmware.py明确选择私有库。

受控变化：Device.begin在clearConfiguration后默认CDC注册前调用fluxrail_usb_wants_cdc（完整保存/恢复人格选择），按需注册CDC及复合类，再调用唯一fluxrail_usb_configure_before_connect注册完整人格，最后Port_InitDevice首次连接；UTF-16缓冲/转换上限126字符；宏FLUXRAIL_TINYUSB_STARTUP_ADAPTER=2；私有fluxrail_cdc_tx_idle同时检查DTR、FIFO为空及真实TX端点空闲。descriptor/get/set回调仍由库唯一持有。

FluxRail-adapter.json记录三份上游及生成哈希、源/目标路径和system_library_modified=false。来源内容即使同版本变更也拒绝生成，需要重新审查。=0默认纯IO4/普通两人格正式ELF描述符执行已有证据；=1带CDC的IO4候选本轮未构建/实测。完整说明见../firmware/FluxRail/README.md与../docs/spec/13_IO4软件契约.md；未上传/实板/游戏验证。

# Orbit Clock AI 固件 · 2026-10-27

适用板卡：Waveshare ESP32-S3-Touch-AMOLED-2.16，16MB Flash / 8MB PSRAM。
包含像素时钟、互动音效、专注、骰子、流沙、玩家转盘、小智语音和动态眼睛。
小智页面已移除屏幕返回按钮，单击 Key3 返回。源代码与完整中文说明见配套 GitHub 发布包。

## 烧录

优先使用完整发布包的 VS Code 工作区，任务会校验五个镜像。此 ZIP 不包含设备 NVS、Wi-Fi 密码或整片备份。

手动烧录时，在本目录安装 esptool，并将下面的 COM3 换成实际端口。Windows 可使用 python 代替 python3。

```sh
python3 -m pip install esptool==5.3.1
python3 -m esptool --chip esp32s3 --port COM3 --baud 115200 --no-stub write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB 0x0 bootloader.bin 0x8000 partition-table.bin 0xe000 ota_data_initial.bin 0x20000 orbit-clock-ai.bin 0x800000 assets.bin
```

主应用地址为 0x20000；不要使用 Arduino 版的 0x10000。NVS 位于 0x9000、大小 0x5000，本包不写该范围，不需要整片擦除。SHA256SUMS 与 flash_manifest.json 提供文件摘要、大小和地址。

首次使用按屏幕提示配网并完成小智激活。三颗按键朝上为正方向；按住 Key3 三秒可进行触摸校准。Key2 用于手动联网或在线重新校时。

本次完成编译与主机检查，尚未通过自动化向实板烧录。第三方代码许可及演示媒体来源见同目录的 THIRD_PARTY_NOTICES.md 与 THIRD_PARTY_LICENSES.txt；用户提供素材不由代码许可证重新授权。

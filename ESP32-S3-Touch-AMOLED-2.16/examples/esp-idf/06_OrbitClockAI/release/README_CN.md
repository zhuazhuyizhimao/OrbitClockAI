# Orbit Clock AI · 像素桌面时钟

基于 **Waveshare ESP32-S3-Touch-AMOLED-2.16** 的自定义固件，集成像素时钟、角色互动音效、专注计时、骰子、流沙倒计时、玩家转盘，以及小智 AI 语音对话。

本发布包对应 **2026-10-27** 版本，包含源码、预编译固件、校验清单和可移植的 VS Code 工作区。小智表情已替换为从视频提取的黑底动态眼睛；语音界面使用 **Key3 返回**，不再显示“返回时钟”屏幕按钮。

![像素时钟界面预览](docs/assets/clock.png)

![小智动态眼睛](docs/assets/eyes.gif)

以上为软件预览和提取素材，不是本次固件的实板测试照片。

## 1. 支持的硬件

| 项目 | 配置 |
| --- | --- |
| 开发板 | Waveshare ESP32-S3-Touch-AMOLED-2.16 |
| 芯片 | ESP32-S3 |
| Flash / PSRAM | 16MB Flash / 8MB PSRAM |
| 屏幕 | 480×480 AMOLED，CO5300 |
| 触摸 | CST9217，组件固定为 1.0.4 |
| 运动传感器 | QMI8658，用于晃动掷骰和方向检测 |
| 声音 | ES8311 / ES7210，由小智 AudioService 统一管理 |
| 联网 | 2.4GHz Wi-Fi |
| 屏幕正方向 | 屏幕朝向自己，三颗物理按键位于上边缘 |

其他板型需要单独适配引脚、屏幕、触摸和音频驱动。

## 2. 功能与操作

| 页面 / 功能 | 操作 |
| --- | --- |
| 像素时钟 | 点击角色切换皮卡丘形态并播放 2.2 秒音效；连续点击不会排队重播 |
| 时钟设置 | 点击时间切换时制；长按时间进入设置，可打开小智 AI |
| 专注计时 | 设置 1～180 分钟，支持开始、暂停和重置 |
| 骰子 | 支持 1～3 颗、2～8 面；2 面时固定一颗正反硬币；晃动后转动 2 秒出结果；长按设置 |
| 流沙倒计时 | 点击页面开始 / 暂停 / 继续；两次切换至少间隔 0.5 秒；底部宽按钮重置；长按屏幕 2 秒进入时长设置，范围 1～3600 秒 |
| 开始玩家 | 长按设置 2～12 人；点击开始转动，先转 3 秒再减速停止，选出一名玩家 |
| 小智 AI | 联网并激活后，说“你好，小智”，或从时钟设置打开 |
| 动态表情 | 黑底眼睛 GIF，400×180、20fps、5.8 秒循环；21 种标准情绪共用这段动画 |
| 自动联网 | 开机优先连接最近成功使用的 Wi-Fi，失败时尝试其他已保存网络 |
| 自动校时 | 联网后请求 NTP 时间，按北京时间显示；也支持 USB 电脑校时 |

进入语音界面时，流沙暂停，骰子 / 转盘取消当前动画；返回后保留原页面。专注倒计时继续走时。退出语音界面后暂停 GIF 解码，减少后台开销。

### 物理按键

| 按键 | 当前用途 |
| --- | --- |
| Key1 | 电源键，由电源管理芯片处理；没有应用自定义单击事件 |
| Key2 / BOOT（GPIO0） | 按下显示联网提示，松开执行联网；离线时重试已保存 Wi-Fi，在线时重新校时；没有单独的双击 / 长按功能 |
| Key3（GPIO18） | 游戏切换、返回，以及手动触摸校准 |

### Key3 导航

| 当前页面 | 单击 Key3 | 双击 Key3 |
| --- | --- | --- |
| 时钟 | 进入骰子 | — |
| 骰子 | 返回时钟 | 进入流沙 |
| 流沙 | 返回骰子 | 进入开始玩家转盘 |
| 开始玩家转盘 | 返回流沙 | — |
| 小智语音 | 结束对话，返回原时钟 / 游戏页面 | — |

在桌面 / 游戏页面**按住 Key3 三秒**可进入三点触摸校准。校准界面单击 Key3 取消并保留旧记录。

## 3. 快速烧录：不需要编译

1. 解压完整发布包，保持目录结构。
2. 使用 USB 数据线连接开发板，关闭占用串口的监视器、网页烧录器和其他烧录工具。
3. 安装 Python 烧录依赖：

   ```sh
   python3 -m pip install -r ESP32-S3-Touch-AMOLED-2.16/requirements-flash.txt
   ```

4. 在 VS Code 打开根目录的 `OrbitClockAI.code-workspace`，选择“终端 → 运行任务”：
   - **小智合并版：烧录并校时（先备份）**：先读取完整 16MB 备份，备份失败会停止。
   - **小智合并版：直接烧录并校时（不备份）**：明确跳过完整备份，使用 ROM 下载器和 115200 波特率写入。

也可以在发布包根目录使用命令行：

```sh
cd ESP32-S3-Touch-AMOLED-2.16
python3 scripts/orbit_ai.py flash --skip-backup
```

默认自动识别一个 ESP32 USB 串口。有多个设备或识别失败时，显式指定端口，例如 Linux 的 `/dev/ttyACM0` 或 Windows 的 `COM3`：

```sh
python3 scripts/orbit_ai.py flash --port COM3 --skip-backup
```

Windows 如果没有 `python3` 命令，使用 `python`。VS Code 工作区已经针对 Windows 使用 `python`，macOS / Linux 使用 `python3`。

烧录完成后脚本尝试通过 USB 校时。若仅校时报错，可先检查板子是否已启动，再关闭其他串口工具，运行“仅 USB 校时”；校时失败不等于固件没有写入。

## 4. 首次启动

### 触摸校准

首次进入流沙页面且没有校准记录时，会显示三点校准流程：

1. 屏幕朝向自己，三颗按键朝上。
2. 依次点击当前显示的圆点，每次点击后松手。
3. 显示完成后自动返回，结果保存到 NVS，断电后有效。

![触摸校准预览](docs/assets/touch-calibration.png)

以后可按住 Key3 三秒重新校准。若触摸位置明显错位，应先完成校准，再判断按钮是否正常。

### Wi-Fi 与小智激活

1. 没有保存网络时，按屏幕提示连接 `Xiaozhi-xxxxxx` 热点。
2. 浏览器访问 `http://192.168.4.1`，设置可访问互联网的 2.4GHz Wi-Fi。
3. 屏幕如显示六位激活码，登录 [小智控制台](https://xiaozhi.me/) 添加设备并输入激活码。
4. 返回时钟后，说“你好，小智”测试唤醒；单击 Key3 结束对话并返回。
5. 下次开机会自动连接已保存网络。单击 Key2 可手动重试联网或重新校时。

每台设备使用自己的配网与激活流程，发布包不包含设备绑定信息。连接路由器或校时成功不代表云端语音服务已激活。

## 5. 固件布局与设置保留

固件位于 [outputs/OrbitClockAI-firmware](outputs/OrbitClockAI-firmware/)，烧录前脚本验证 SHA-256、大小、地址及分区表。

| 文件 | 写入地址 | 用途 |
| --- | --- | --- |
| `bootloader.bin` | `0x0` | 启动程序 |
| `partition-table.bin` | `0x8000` | 分区表 |
| `ota_data_initial.bin` | `0xe000` | OTA 初始选择数据 |
| `orbit-clock-ai.bin` | `0x20000` | 主应用 |
| `assets.bin` | `0x800000` | 唤醒模型、中文字体和 GIF |

**主应用地址是 `0x20000`。** 请使用本发布包的任务与清单，不要套用 Arduino 版的 `0x10000`。

NVS 位于 `0x9000`，大小 `0x5000`。发布包没有 `nvs.bin`，烧录任务不写该范围，也不执行整片擦除，用于保留已有 Wi-Fi、游戏和校准设置。使用其他分区布局的旧固件时，是否兼容其历史设置需要另行确认。

本版关闭网络固件升级，以保留自定义时钟与游戏；小智服务器发现、激活和在线对话保留。后续更新通过 USB 烧录。

## 6. 源码与重新编译

```text
.
├── README.md / README_CN.md
├── OrbitClockAI.code-workspace
├── LICENSE / THIRD_PARTY_NOTICES.md / LICENSES/
├── docs/                         # 软件预览、验证记录、隐私检查
├── outputs/OrbitClockAI-firmware/ # 五个镜像与校验清单
└── ESP32-S3-Touch-AMOLED-2.16/
    ├── scripts/                  # 准备、编译、烧录、校验与打包脚本
    ├── tests/                    # 自定义功能主机检查
    └── examples/
        ├── arduino/08_OrbitClock/ # 共享时钟和游戏源码、生成资源
        ├── arduino/libraries/    # AI 构建需要的 SensorLib 和字体源数据
        └── esp-idf/06_OrbitClockAI/
            ├── board/            # 本板驱动、输入、网络和语音界面
            ├── assets/           # 动态眼睛 GIF
            └── network/          # 本地 Wi-Fi 组件适配
```

发布包提供合并版需要的依赖源文件；保留的 Arduino 目录主要用于共享 UI，不是完整 Arduino SDK / 全部板库发行包。

### 构建环境

- Git、Python 和官方 ESP-IDF **6.0.1 或更新版本**；推荐 6.1。
- 本次预编译固件使用 ESP-IDF **6.0.1** 和对应 Xtensa 工具链。
- 首次准备和构建需要联网下载固定版本小智源码及组件。
- 小智基线固定为 v2.5.2，提交 `49b4380b3faa0ceadd28a3e78aaf0f311c46c57e`，自定义板型为 `orbit-clock-ai`。
- 本次编译验证环境为 macOS；预编译烧录任务提供 Windows 命令配置。现有重编译脚本按类 Unix 工具目录组织，Windows 重编译建议使用 WSL / Linux 环境，未声称已通过原生 Windows 构建。

先按 ESP-IDF 官方安装流程准备开发环境，再进入已激活的 ESP-IDF 终端执行：

```sh
cd ESP32-S3-Touch-AMOLED-2.16
python3 scripts/orbit_ai.py prepare
python3 scripts/orbit_ai.py build
```

脚本读取 `IDF_PATH`、`IDF_TOOLS_PATH`、`IDF_PYTHON_ENV_PATH`。VS Code 编译任务也需要从已配置好 ESP-IDF 环境的窗口运行。编译产物更新到上层 `outputs/OrbitClockAI-firmware/`。

官方小智工程与缓存生成到上层 `work/`。修改本仓库的 `board/`、共享 UI 或生成脚本；直接修改 `work/xiaozhi-upstream` 会在下次准备时被覆盖。发布构建通过编译器路径映射移除个人目录，不对固件做二进制字符串修补。

### 修改表情

替换 `examples/esp-idf/06_OrbitClockAI/assets/screen-eyes.gif` 后重新编译。GIF 尺寸不得超过 480×480，建议使用适合状态栏和字幕布局的尺寸。

`scripts/generate_orbit_ai_assets.py` 参考 [xiaozhi-assets-generator](https://github.com/78/xiaozhi-assets-generator) 的索引格式，以固定小智版本的官方打包器合并 GIF；保留原唤醒模型、字体和字体元数据。当前配置的标准情绪统一使用同一段 GIF。

生成后的中文小字模已经提供，普通构建无需重新生成。重新生成字模时，需要自行准备有相应使用权的字体，并配置字体生成脚本的 `SOURCE` 路径；发布包不附带原始 SimSun 字体文件。

## 7. 检查与验证范围

```sh
cd ESP32-S3-Touch-AMOLED-2.16
python3 -m unittest discover -s tests -v
python3 scripts/orbit_ai.py test
python3 scripts/check_orbit_ai_ui.py
python3 scripts/check_orbit_ai_emotion.py
```

原生 UI 检查需要 clang / clang++；GIF 逐像素检查另外需要 Pillow。先完成合并版构建和 UI 检查，再运行表情检查。

详细记录见 [docs/VALIDATION_CN.md](docs/VALIDATION_CN.md)。已有检查覆盖界面和模型、四向触摸、游戏设置、Key3 路由、音效去重、联网 / 校时逻辑、资源打包以及真实小智 GIF 解码器。**本次发布包未由自动化流程向实板烧录，也没有把主机模拟视为实板验证。** 真实唤醒率、音频、云端会话、无线稳定性和语音并行帧率仍需上板确认。

## 8. 隐私与上传 GitHub

发布目录和最终 ZIP 均经过检查，报告见 [docs/PRIVACY_REPORT.json](docs/PRIVACY_REPORT.json)。

- 不包含 `clock_secrets.h`、实际 Wi-Fi 配置、设备备份、NVS 镜像或设备绑定数据。
- 不包含个人目录路径、串口日志、ELF / MAP 调试文件、构建缓存或私钥。
- 仅保留 `clock_secrets.example.h` 中的示例占位值；小智合并版在设备配网页面保存 Wi-Fi。
- 根目录 `.gitignore` 排除运行后生成的 `work/`、备份、缓存与本地凭据。

可以把**本目录内部的文件**上传到自己的 GitHub 仓库。默认首页 `README.md` 已链接本中文文档；如希望直接显示完整中文说明，可将本文件内容复制到 `README.md`。独立固件 ZIP 可作为 GitHub Release 附件发布。此包没有代为创建仓库、提交或上传。

重新检查发布目录或 ZIP：

```sh
python3 ESP32-S3-Touch-AMOLED-2.16/scripts/check_release_privacy.py .
```

该检查用于整理好的发布目录；重新构建后产生的本地 `work/`、日志和设备备份不应加入上传内容。

## 9. 来源与许可证

工程基于 [Waveshare 板卡示例](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.16) 与 [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) 开发。保留原仓库 Apache-2.0 许可证和第三方组件的独立许可证，详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

皮卡丘角色图片、音效片段及视频提取的眼睛 GIF 来自用户提供素材，未随素材提供独立的再分发授权文件，**不纳入代码许可证授予范围**。公开发布前应确认素材使用授权，或换成原创 / 明确授权的素材。生成字模的来源也在第三方说明中单独记录。

参考资料：

- [Waveshare 板卡文档](https://docs.waveshare.net/ESP32-S3-Touch-AMOLED-2.16/)
- [Waveshare 小智教程](https://docs.waveshare.net/ESP32-S3-Touch-AMOLED-2.16/ESP32-AI-Tutorials/)
- [小智固件源码](https://github.com/78/xiaozhi-esp32)
- [小智资源生成器](https://github.com/78/xiaozhi-assets-generator)

# 第三方来源与素材说明

本包保留上游声明与源码文件内的版权头。根目录 `LICENSE` 来自 Waveshare 仓库，为 Apache-2.0；第三方组件及素材不因放入本包而改变各自许可条件。

| 内容 | 来源 / 说明 |
| --- | --- |
| Waveshare 板卡工程 | [waveshareteam/ESP32-S3-Touch-AMOLED-2.16](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.16)，Apache-2.0，根目录 LICENSE |
| 小智固件基线 | [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)，v2.5.2 / `49b4380b3faa0ceadd28a3e78aaf0f311c46c57e`，MIT，见 LICENSES/XiaoZhi-MIT.txt |
| 本地 Wi-Fi 组件 | esp-wifi-connect 3.3.1，MIT；保留组件 LICENSE 与 ORBIT_CHANGES.md |
| SensorLib | Waveshare 示例携带的 SensorLib 源码，保留 LICENSE 和源码内其他许可头 |
| LVGL 与 Montserrat 数字字形 | 保留 LVGL 许可证；字体许可说明见 LICENSES/Montserrat-OFL.txt |
| Arduino GFX CO5300 参考代码 | 仅附屏幕方向对照测试用的驱动源码，保留库内 license.txt |
| ESP-IDF 与实际参与构建的组件 | 许可文本收集到 LICENSES/；组件通过 ESP-IDF Component Manager 在构建时获取 |
| 小智字体 / 唤醒模型 | 沿用固定版本默认资源生成流程及依赖自身的许可；本包没有重新授权模型或字库 |

## 用户提供的演示素材

以下素材随当前功能保留，未随输入材料提供独立的再分发授权文件，不属于代码许可证所授予的权利：

- 皮卡丘角色像素图、其 `.pixels` 数据和生成的 `retro_assets.c`。
- 皮卡丘 2.2 秒声音片段及生成的 WAV / PCM / Ogg 数据。
- 从用户视频屏幕中提取的动态眼睛 GIF。
- 基于原工程所带 SimSun 字体生成的中文子集字模与硬币汉字点阵。原始 SimSun 字体文件未放入发布包；相关生成脚本保留来源说明。

发布者应确认这些素材适用的授权，或替换为原创 / 明确授权素材。此说明用于记录来源，不构成素材权利人的授权证明。

## 本次适配

自定义板型 `orbit-clock-ai`、桌面时钟 / 游戏集成、触摸稳定性与校准、联网 / 校时策略、语音返回逻辑、自定义表情资源生成及发布脚本，均在对应维护源码中提供。生成的 `work/`、上游下载缓存、设备备份和本地配置不属于本发布包。

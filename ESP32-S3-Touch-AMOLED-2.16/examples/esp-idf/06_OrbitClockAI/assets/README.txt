screen-eyes.gif 来自用户提供视频中的屏幕表情，机械提取并清除外壳、状态文字和介绍文字。
400×180，116 帧，20 fps，5.8 秒，无限循环，黑色背景。仅裁去空白区域以适配小智界面，没有重新绘制表情。
使用 xiaozhi-assets-generator 的 index.json / emoji_collection / skin 结构，由当前锁定小智版本的官方资源打包器生成 assets.bin。
全部情绪使用同一张 GIF，并非 21 种不同表情。保留默认资源中的唤醒模型、字体及元数据。
参考：https://github.com/78/xiaozhi-assets-generator

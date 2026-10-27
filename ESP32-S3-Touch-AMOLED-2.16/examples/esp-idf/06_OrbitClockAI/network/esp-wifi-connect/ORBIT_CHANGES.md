# Orbit Wi-Fi component fork

Based on [78/esp-wifi-connect 3.3.1](https://github.com/78/esp-wifi-connect/tree/cc103899ea8199b0fbf18c9b6e65152d9a92091e), commit `cc103899ea8199b0fbf18c9b6e65152d9a92091e`, under its included MIT license. The upstream README and code remain for attribution. Screenshot assets used only by the upstream README are omitted.

Only `wifi_station.cc` changes behavior:

- Sort the scanned connection queue by saved SSID order, retaining strongest-BSSID order within each SSID and all saved fallback credentials.
- If the preferred SSID is missing on its saved channel, scan all channels before selecting another saved SSID.
- After obtaining an IP address, move that successful SSID to the front using the existing `SsidManager::SetDefaultSsid` persistent API. Avoid NVS writes when already first. Failed attempts never become the preferred SSID.

`scripts/orbit_ai.py prepare` selects this maintained source directory using the component manager's `override_path`. The downloaded managed component and component cache remain untouched. It is local to the combined project; the original Arduino firmware is unaffected. The native network regression tests execute the actual scan-selection code and successful-connection promotion.

本地修改：开机优先连接最近成功联网的 Wi-Fi，失败时保留其他已保存网络作为候选；热点换信道时先全信道扫描。仅成功获取 IP 后持久化优先顺序，不删除其他网络密码。通过组件管理器的本地路径覆盖构建，未修改下载缓存。

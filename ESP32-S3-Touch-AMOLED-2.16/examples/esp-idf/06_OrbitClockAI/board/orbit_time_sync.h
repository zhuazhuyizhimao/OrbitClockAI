#pragma once
#include <esp_netif_sntp.h>
#include <atomic>
#include <cstdint>
#include "orbit_network_state.h"

namespace orbit_clock_ai {
inline std::atomic<bool> ntp_reply_received{false};

// Called from the GUI tick; network requests/callbacks only exchange atomics.
// Never wait for DNS, an NTP response or a Wi-Fi connection on the GUI task.
class NetworkClock {
public:
    NetworkNotice Poll(bool online, uint32_t now, bool request) {
        if (!online) {
            was_online_ = waiting_ = false;
            ntp_reply_received.store(false);
            return NetworkNotice::None;
        }
        const bool just_connected = !was_online_;
        was_online_ = true;
        if (ntp_reply_received.exchange(false)) {
            waiting_ = false;
            needs_retry_ = false;
            return NetworkNotice::Synced;
        }
        if (waiting_) {
            if (uint32_t(now - last_attempt_) >= 30000) {
                waiting_ = false;
                needs_retry_ = true;
                return NetworkNotice::SyncFailed;
            }
            return request ? NetworkNotice::Syncing : NetworkNotice::None;
        }
        if (!just_connected && !request &&
            !(needs_retry_ && uint32_t(now - last_attempt_) >= 60000))
            return NetworkNotice::None;
        last_attempt_ = now;
        needs_retry_ = true;
        if (!initialized_) {
            esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(
                3, ESP_SNTP_SERVER_LIST("ntp.aliyun.com", "ntp.tencent.com", "pool.ntp.org"));
            config.start = false;
            config.wait_for_sync = false;
            config.sync_cb = [](struct timeval* tv) {
                if (tv && tv->tv_sec > 1700000000)
                    ntp_reply_received.store(true);
            };
            if (esp_netif_sntp_init(&config) != ESP_OK)
                return NetworkNotice::SyncFailed;
            initialized_ = true;
        }
        if (esp_netif_sntp_start() != ESP_OK)
            return NetworkNotice::SyncFailed;
        waiting_ = true;
        return NetworkNotice::Syncing;
    }

private:
    bool initialized_ = false, was_online_ = false, waiting_ = false, needs_retry_ = false;
    uint32_t last_attempt_ = 0;
};

inline const char* NetworkNoticeText(NetworkNotice notice) {
    switch (notice) {
        case NetworkNotice::Starting:
            return "网络正在启动";
        case NetworkNotice::Connecting:
            return "正在连接 Wi-Fi";
        case NetworkNotice::Config:
            return "请按提示配置 Wi-Fi";
        case NetworkNotice::Syncing:
            return "Wi-Fi 已连接，正在校时";
        case NetworkNotice::Synced:
            return "网络校时成功";
        case NetworkNotice::SyncFailed:
            return "校时未成功，稍后重试";
        case NetworkNotice::Offline:
            return "网络已断开，正在重连";
        case NetworkNotice::RetryFailed:
            return "网络操作未完成，请重试";
        case NetworkNotice::ManualRequested:
            return "Key2：检查网络";
        default:
            return "";
    }
}
}  // namespace orbit_clock_ai

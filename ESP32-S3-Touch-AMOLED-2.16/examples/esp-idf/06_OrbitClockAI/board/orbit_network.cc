#include "orbit_network.h"
#include <esp_log.h>
#include <ssid_manager.h>
#include <wifi_manager.h>
#include "application.h"
#include "assets/lang_config.h"
#include "display.h"
#include "orbit_network_state.h"

namespace {
std::atomic<orbit_clock_ai::NetworkNotice> pending_notice{orbit_clock_ai::NetworkNotice::None};
std::atomic<bool> sync_requested{false};
std::atomic<bool> network_online{false};
std::atomic<bool> setup_requested{false};
}  // namespace
namespace orbit_clock_ai {
void PostNetworkNotice(NetworkNotice notice) { pending_notice.store(notice); }
NetworkNotice TakeNetworkNotice() { return pending_notice.exchange(NetworkNotice::None); }
void RequestClockSync() { sync_requested.store(true); }
bool TakeClockSyncRequest() { return sync_requested.exchange(false); }
bool NetworkOnline() { return network_online.load(); }
void RequestNetworkSetupUi() { setup_requested.store(true); }
bool TakeNetworkSetupUiRequest() { return setup_requested.exchange(false); }
}  // namespace orbit_clock_ai

void OrbitWifiBoard::SetNetworkEventCallback(NetworkEventCallback callback) {
    WifiBoard::SetNetworkEventCallback(
        [callback = std::move(callback)](NetworkEvent event, const std::string& data) {
            using namespace orbit_clock_ai;
            switch (event) {
                case NetworkEvent::Connected:
                    network_online.store(true);
                    RequestClockSync();
                    break;
                case NetworkEvent::Scanning:
                case NetworkEvent::Connecting:
                    network_online.store(false);
                    PostNetworkNotice(NetworkNotice::Connecting);
                    break;
                case NetworkEvent::Disconnected:
                    network_online.store(false);
                    PostNetworkNotice(NetworkNotice::Offline);
                    break;
                case NetworkEvent::WifiConfigModeEnter:
                    network_online.store(false);
                    PostNetworkNotice(NetworkNotice::Config);
                    break;
                default:
                    break;
            }
            // Preserve activation and audio-channel lifecycle on every reconnect.
            if (callback)
                callback(event, data);
        });
}

void OrbitWifiBoard::ConnectManually() {
    using namespace orbit_clock_ai;
    if (manual_busy_.exchange(true))
        return;
    if (xTaskCreate(
            [](void* context) {
                auto* board = static_cast<OrbitWifiBoard*>(context);
                board->ConnectWorker();
                board->manual_busy_.store(false);
                vTaskDelete(nullptr);
            },
            "orbit_wifi", 4096, this, 2, nullptr) != pdPASS) {
        manual_busy_.store(false);
        PostNetworkNotice(NetworkNotice::RetryFailed);
        ESP_LOGE("OrbitNetwork", "Unable to create connection worker");
    }
}

void OrbitWifiBoard::ConnectWorker() {
    using namespace orbit_clock_ai;
    auto& wifi = WifiManager::GetInstance();
    if (!wifi.IsInitialized()) {
        PostNetworkNotice(NetworkNotice::Starting);
        return;
    }
    if (wifi.IsConnected()) {
        RequestClockSync();
        return;
    }
    PostNetworkNotice(NetworkNotice::Connecting);
    const bool saved = !SsidManager::GetInstance().GetSsidList().empty();
    if (wifi.IsConfigMode()) {
        if (saved) {
            // ConfigModeExit invokes WifiBoard::TryWifiConnect exactly once.
            esp_timer_stop(connect_timer_);
            wifi.StopConfigAp();
        } else {
            PostNetworkNotice(NetworkNotice::Config);
            RequestNetworkSetupUi();
            Application::GetInstance().Schedule([&wifi]() {
                std::string hint = Lang::Strings::CONNECT_TO_HOTSPOT + wifi.GetApSsid();
                hint += Lang::Strings::ACCESS_VIA_BROWSER;
                hint += wifi.GetApWebUrl();
                Application::GetInstance().Alert(Lang::Strings::WIFI_CONFIG_MODE, hint.c_str(),
                                                 "gear", Lang::Sounds::OGG_WIFICONFIG);
            });
        }
        return;
    }
    if (!saved) {
        RequestNetworkSetupUi();
        Application::GetInstance().Schedule([this]() { EnterWifiConfigMode(); });
        return;
    }
    if (esp_timer_is_active(connect_timer_))
        return;  // Repeated clicks must not reset a live connection attempt.
    wifi.StopStation();
    TryWifiConnect();
}

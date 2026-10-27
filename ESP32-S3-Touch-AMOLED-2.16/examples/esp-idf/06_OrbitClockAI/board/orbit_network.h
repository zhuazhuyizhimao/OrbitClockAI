#pragma once
#include <atomic>
#include "wifi_board.h"

// Use the same manager and connected event path as XiaoZhi activation/protocols.
class OrbitWifiBoard : public WifiBoard {
public:
    void SetNetworkEventCallback(NetworkEventCallback callback) override;
    void ConnectManually();

private:
    std::atomic<bool> manual_busy_{false};
    void ConnectWorker();
};

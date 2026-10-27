#pragma once

namespace orbit_clock_ai {
enum class NetworkNotice {
    None,
    Starting,
    Connecting,
    Config,
    Syncing,
    Synced,
    SyncFailed,
    Offline,
    RetryFailed,
    ManualRequested
};
void PostNetworkNotice(NetworkNotice notice);
NetworkNotice TakeNetworkNotice();
void RequestClockSync();
bool TakeClockSyncRequest();
bool NetworkOnline();
void RequestNetworkSetupUi();
bool TakeNetworkSetupUiRequest();
}  // namespace orbit_clock_ai

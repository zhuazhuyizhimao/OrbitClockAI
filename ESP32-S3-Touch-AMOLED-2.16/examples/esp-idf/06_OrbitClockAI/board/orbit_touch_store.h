#pragma once
#include <nvs.h>
#include "orbit_touch_mapping.h"

namespace orbit_touch {
extern Mapping mapping;
inline void LoadCalibration() {
    nvs_handle_t handle;
    if (nvs_open("orbit-touch", NVS_READONLY, &handle) != ESP_OK)
        return;
    Mapping candidate;
    size_t size = sizeof(candidate);
    const auto result = nvs_get_blob(handle, "matrix", &candidate, &size);
    nvs_close(handle);
    if (result == ESP_OK && size == sizeof(candidate) && candidate.Valid())
        mapping = candidate;
}
inline bool SaveCalibration(const Mapping& candidate) {
    if (!candidate.Valid())
        return false;
    nvs_handle_t handle;
    if (nvs_open("orbit-touch", NVS_READWRITE, &handle) != ESP_OK)
        return false;
    auto result = nvs_set_blob(handle, "matrix", &candidate, sizeof(candidate));
    if (result == ESP_OK)
        result = nvs_commit(handle);
    nvs_close(handle);
    if (result != ESP_OK)
        return false;
    mapping = candidate;
    return true;
}
}  // namespace orbit_touch

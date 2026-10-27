#pragma once
#include <math.h>
#include "orbit_display.h"
// SensorLib must use the shared ESP-IDF I2C master bus, never the legacy driver.
#define CONFIG_SENSORLIB_ESP_IDF_NEW_API 1
#include <driver/gpio.h>
#include <esp_random.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <fcntl.h>
#include <nvs.h>
#include <sys/time.h>
#include <unistd.h>
#include <SensorQMI8658.hpp>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include "orbit_network_state.h"

inline uint32_t millis() { return uint32_t(esp_timer_get_time() / 1000); }
constexpr int WL_CONNECTED = 1;
struct OrbitWifi {
    int status() const { return orbit_clock_ai::NetworkOnline() ? WL_CONNECTED : 0; }
};
static OrbitWifi WiFi;
struct OrbitConsole {
    int buffered = -1;
    bool available() {
        if (buffered >= 0)
            return true;
        unsigned char c;
        if (::read(STDIN_FILENO, &c, 1) == 1)
            buffered = c;
        return buffered >= 0;
    }
    char read() {
        const char c = char(buffered);
        buffered = -1;
        return c;
    }
    void println(const char* s) { std::printf("%s\n", s); }
    void printf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        std::vprintf(format, args);
        va_end(args);
    }
};
static OrbitConsole USBSerial;

// Keep the existing NVS namespaces and value types so current game preferences
// remain compatible when the combined firmware is flashed without erasing NVS.
class Preferences {
    nvs_handle_t handle_ = 0;

public:
    bool begin(const char* name, bool read_only) {
        return nvs_open(name, read_only ? NVS_READONLY : NVS_READWRITE, &handle_) == ESP_OK;
    }
    void end() {
        if (handle_)
            nvs_close(handle_);
        handle_ = 0;
    }
    size_t putUShort(const char* key, uint16_t value) {
        return nvs_set_u16(handle_, key, value) == ESP_OK && nvs_commit(handle_) == ESP_OK ? 2 : 0;
    }
    size_t putUChar(const char* key, uint8_t value) {
        return nvs_set_u8(handle_, key, value) == ESP_OK && nvs_commit(handle_) == ESP_OK ? 1 : 0;
    }
    size_t putBool(const char* key, bool value) { return putUChar(key, value); }
    size_t putBytes(const char* key, const void* value, size_t size) {
        return nvs_set_blob(handle_, key, value, size) == ESP_OK && nvs_commit(handle_) == ESP_OK
                   ? size
                   : 0;
    }
    uint16_t getUShort(const char* key, uint16_t fallback) {
        uint16_t value;
        return nvs_get_u16(handle_, key, &value) == ESP_OK ? value : fallback;
    }
    uint8_t getUChar(const char* key, uint8_t fallback) {
        uint8_t value;
        return nvs_get_u8(handle_, key, &value) == ESP_OK ? value : fallback;
    }
    bool getBool(const char* key, bool fallback) { return getUChar(key, fallback) != 0; }
    size_t getBytesLength(const char* key) {
        size_t size = 0;
        return nvs_get_blob(handle_, key, nullptr, &size) == ESP_OK ? size : 0;
    }
    size_t getBytes(const char* key, void* value, size_t size) {
        return nvs_get_blob(handle_, key, value, &size) == ESP_OK ? size : 0;
    }
};

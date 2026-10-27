#pragma once

// Copy clock_secrets.example.h to clock_secrets.h to enable Wi-Fi time sync.
// Without credentials, the UI and focus timer work; wall time stays unset.
#if __has_include("clock_secrets.h")
#include "clock_secrets.h"
#else
#define CLOCK_WIFI_SSID ""
#define CLOCK_WIFI_PASSWORD ""
#endif

// POSIX time-zone rule: UTC+8 (China), without daylight saving time.
#define CLOCK_TIMEZONE "CST-8"
#define CLOCK_NTP_PRIMARY "pool.ntp.org"
#define CLOCK_NTP_SECONDARY "time.nist.gov"

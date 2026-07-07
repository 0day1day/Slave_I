#pragma once

#include <string>
#include <vector>

#include "core/clock.hpp"

namespace spectra5::domain {

enum class WifiBand {
    GHz24,
    GHz5,
};

enum class WifiSecurity {
    Open,
    Wep,
    Wpa,
    Wpa2,
    Wpa3,
    WpaEnterprise,
};

inline const char* wifi_band_name(WifiBand b)
{
    return b == WifiBand::GHz24 ? "2.4 GHz" : "5 GHz";
}

inline const char* wifi_security_name(WifiSecurity s)
{
    switch (s) {
        case WifiSecurity::Open:          return "Open";
        case WifiSecurity::Wep:           return "WEP";
        case WifiSecurity::Wpa:           return "WPA";
        case WifiSecurity::Wpa2:          return "WPA2";
        case WifiSecurity::Wpa3:          return "WPA3";
        case WifiSecurity::WpaEnterprise: return "WPA-Enterprise";
    }
    return "Open";
}

// A single observed access point. rssi_history keeps the most recent samples so
// the detail view can render a signal timeline.
struct WifiAccessPoint {
    std::string bssid;
    std::string ssid;
    int channel        = 1;
    WifiBand band      = WifiBand::GHz24;
    int rssi           = -90;
    WifiSecurity security = WifiSecurity::Open;
    bool hidden        = false;
    Timestamp first_seen = 0;
    Timestamp last_seen  = 0;
    std::vector<int> rssi_history;
};

}  // namespace spectra5::domain

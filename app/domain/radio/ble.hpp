#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/clock.hpp"

namespace spectra5::domain {

enum class BleAddressType {
    Public,
    Random,
    Unknown,
};

inline const char* ble_address_type_name(BleAddressType t)
{
    switch (t) {
        case BleAddressType::Public:  return "public";
        case BleAddressType::Random:  return "random";
        case BleAddressType::Unknown: return "unknown";
    }
    return "unknown";
}

struct BleAdvertisement {
    std::string address;
    BleAddressType address_type = BleAddressType::Unknown;
    std::string name;
    std::string vendor;  // from the advertisement's manufacturer company ID
    int rssi                    = -127;
    Timestamp first_seen        = 0;
    Timestamp last_seen         = 0;
    std::vector<int> rssi_history;
};

}  // namespace spectra5::domain

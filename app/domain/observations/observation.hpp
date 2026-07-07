#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/clock.hpp"
#include "domain/sessions/session.hpp"

namespace spectra5::domain {

using ObservationId = std::string;
using SourceId      = std::string;
using MetadataMap   = std::map<std::string, std::string>;

enum class ObservationType {
    WifiAp,
    BleDevice,
    Ieee802154,
    Rf,
    Nfc,
    Generic,
};

inline const char* observation_type_name(ObservationType t)
{
    switch (t) {
        case ObservationType::WifiAp:     return "wifi";
        case ObservationType::BleDevice:  return "ble";
        case ObservationType::Ieee802154: return "ieee802154";
        case ObservationType::Rf:         return "rf";
        case ObservationType::Nfc:        return "nfc";
        case ObservationType::Generic:    return "generic";
    }
    return "generic";
}

inline ObservationType observation_type_from(const std::string& s)
{
    if (s == "wifi") return ObservationType::WifiAp;
    if (s == "ble") return ObservationType::BleDevice;
    if (s == "ieee802154") return ObservationType::Ieee802154;
    if (s == "rf") return ObservationType::Rf;
    if (s == "nfc") return ObservationType::Nfc;
    return ObservationType::Generic;
}

// A single observation captured during a session (PRD §23). Schema version is
// preserved so exports remain interpretable across firmware revisions.
struct Observation {
    static constexpr int kSchemaVersion = 1;

    ObservationId id;
    SessionId session_id;
    ObservationType type = ObservationType::Generic;
    Timestamp timestamp  = 0;
    SourceId source;
    int signal_strength = 0;
    std::vector<uint8_t> raw_data;
    MetadataMap metadata;
};

}  // namespace spectra5::domain

#include "services/capabilities/capability.hpp"

namespace spectra5::services {

const char* capability_label(Capability c)
{
    switch (c) {
        case Capability::None:           return "None";
        case Capability::WifiScan:       return "Wi-Fi Scan";
        case Capability::WifiMonitor:    return "Wi-Fi Monitor";
        case Capability::BleScan:        return "BLE Scan";
        case Capability::BleGattClient:  return "BLE GATT Client";
        case Capability::Ieee802154Scan: return "802.15.4 Scan";
        case Capability::StorageSd:      return "microSD";
        case Capability::UsbHost:        return "USB Host";
        case Capability::Rs485:          return "RS485";
        case Capability::Infrared:       return "Infrared";
        case Capability::Cc1101:         return "CC1101";
        case Capability::Nfc:            return "NFC";
        case Capability::Rfid125Khz:     return "RFID 125kHz";
        case Capability::Gps:            return "GPS";
        case Capability::Camera:         return "Camera";
        case Capability::Audio:          return "Audio";
        case Capability::Count:          return "";
    }
    return "";
}

}  // namespace spectra5::services

#pragma once

#include <cstdint>
#include <initializer_list>

// Registro de capacidades de plataforma (sección 8.5 del PRD).
// Capa pura, sin LVGL/ESP-IDF.
namespace spectra5::services {

enum class Capability : std::uint8_t {
    None = 0,
    WifiScan,
    WifiMonitor,
    BleScan,
    BleGattClient,
    Ieee802154Scan,
    StorageSd,
    UsbHost,
    Rs485,
    Infrared,
    Cc1101,
    Nfc,
    Rfid125Khz,
    Gps,
    Camera,
    Audio,
    Count,
};

// Conjunto de capacidades como bitset compacto.
class CapabilitySet {
public:
    CapabilitySet() = default;
    CapabilitySet(std::initializer_list<Capability> caps)
    {
        for (auto c : caps) {
            add(c);
        }
    }

    void add(Capability c)
    {
        if (c != Capability::None) {
            bits_ |= mask(c);
        }
    }

    void remove(Capability c) { bits_ &= ~mask(c); }

    bool has(Capability c) const
    {
        return c == Capability::None || (bits_ & mask(c)) != 0;
    }

    void clear() { bits_ = 0; }

private:
    static std::uint32_t mask(Capability c)
    {
        return static_cast<std::uint32_t>(1) << static_cast<std::uint8_t>(c);
    }

    std::uint32_t bits_ = 0;
};

const char* capability_label(Capability c);

}  // namespace spectra5::services

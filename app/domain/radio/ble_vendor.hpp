#pragma once

#include <cstdint>

namespace spectra5::domain {

// Bluetooth SIG "Company Identifier" (first 2 bytes of manufacturer-specific data,
// little-endian) -> vendor name. Returns "" if unknown. Header-only so any component
// can use it without a link dependency. Curated subset of the ones seen scanning.
inline const char* ble_company_vendor(uint16_t company_id)
{
    struct Entry {
        uint16_t id;
        const char* vendor;
    };
    static constexpr Entry kTable[] = {
        {0x004C, "Apple"},     {0x0075, "Samsung"},   {0x0006, "Microsoft"},
        {0x00E0, "Google"},    {0x0087, "Garmin"},    {0x0157, "Tile"},
        {0x009E, "Bose"},      {0x012D, "Sony"},      {0x038F, "Xiaomi"},
        {0x027D, "Huawei"},    {0x0171, "Amazon"},    {0x03DA, "Logitech"},
        {0x0059, "Nordic"},    {0x0499, "Ruuvi"},     {0x0001, "Ericsson"},
        {0x004F, "u-blox"},    {0x0118, "Ubiquiti"},  {0x0131, "Cypress"},
        {0x0030, "ST Micro"},  {0x000D, "TI"},        {0x0822, "Adafruit"},
        {0x05A7, "Sonos"},     {0x0107, "Polar"},     {0x01D7, "Qingping"},
        {0x060A, "Govee"},     {0x02FF, "Espressif"}, {0x0A12, "Anker"},
        {0x0469, "Withings"},
    };
    for (const auto& e : kTable) {
        if (e.id == company_id) {
            return e.vendor;
        }
    }
    return "";
}

}  // namespace spectra5::domain

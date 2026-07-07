#include "domain/radio/oui.hpp"

#include <cstdint>

namespace spectra5::domain {
namespace {

struct OuiEntry {
    uint32_t oui;  // first 3 bytes, big-endian
    const char* vendor;
};

// Curated set of common consumer OUIs. Each vendor owns many OUIs in reality; these
// are a handful of the most frequently seen so the lists show something useful.
constexpr OuiEntry kTable[] = {
    {0x3C15C2, "Apple"},     {0xF01898, "Apple"},     {0xACBC32, "Apple"},
    {0x04D3CF, "Apple"},     {0xA45E60, "Apple"},     {0xDC2B2A, "Apple"},
    {0x000393, "Apple"},     {0x88665A, "Apple"},     {0x7CD1C3, "Apple"},
    {0xE85B5B, "Samsung"},   {0x5CF6DC, "Samsung"},   {0x8425DB, "Samsung"},
    {0x002566, "Samsung"},   {0xC8190F, "Samsung"},   {0x3C5A37, "Samsung"},
    {0x001A11, "Google"},    {0x3C5AB4, "Google"},    {0xF4F5E8, "Google"},
    {0x18B430, "Nest/Google"},
    {0x001632, "Intel"},     {0x3C9706, "Intel"},     {0xA0A8CD, "Intel"},
    {0x7CB27D, "Intel"},     {0x00BB3A, "Amazon"},    {0xFC65DE, "Amazon"},
    {0x68543D, "Amazon"},    {0x44650D, "Amazon"},    {0x008701, "Microsoft"},
    {0x000D3A, "Microsoft"}, {0x7C1E52, "Microsoft"}, {0x286C07, "Xiaomi"},
    {0x64B473, "Xiaomi"},    {0x9C99A0, "Xiaomi"},    {0x286ED4, "Huawei"},
    {0x002568, "Huawei"},    {0x48435A, "Huawei"},    {0xB827EB, "Raspberry Pi"},
    {0xDCA632, "Raspberry Pi"}, {0xE45F01, "Raspberry Pi"},
    {0x18FE34, "Espressif"}, {0x240AC4, "Espressif"}, {0x3C71BF, "Espressif"},
    {0xA4CF12, "Espressif"}, {0x7C9EBD, "Espressif"}, {0x50C7BF, "TP-Link"},
    {0x1C61B4, "TP-Link"},   {0xAC84C6, "TP-Link"},   {0x0019E0, "TP-Link"},
    {0xC025E9, "TP-Link"},   {0x9C5322, "Cisco"},     {0x00000C, "Cisco"},
    {0x2C3033, "Netgear"},   {0xA040A0, "Netgear"},   {0x44073C, "Sony"},
    {0xFCF152, "Sony"},      {0x000272, "Cisco-Meraki"},
};

}  // namespace

const char* oui_vendor(const MacAddr& mac)
{
    if ((mac[0] & 0x02) != 0) {
        return "(random)";  // locally administered = randomized MAC
    }
    const uint32_t oui = (static_cast<uint32_t>(mac[0]) << 16) |
                         (static_cast<uint32_t>(mac[1]) << 8) | static_cast<uint32_t>(mac[2]);
    for (const auto& e : kTable) {
        if (e.oui == oui) {
            return e.vendor;
        }
    }
    return "";
}

}  // namespace spectra5::domain

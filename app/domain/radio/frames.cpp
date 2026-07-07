#include "domain/radio/frames.hpp"

#include <algorithm>

namespace spectra5::domain {
namespace {

// Shared layout for deauth/disassoc (identical apart from the subtype byte).
ManagementFrame build_mgmt(uint8_t subtype, const MacAddr& bssid, const MacAddr& target,
                           uint16_t reason)
{
    ManagementFrame f{};
    f[0] = subtype;  // frame control: type/subtype
    f[1] = 0x00;
    f[2] = 0x00;  // duration
    f[3] = 0x00;
    std::copy(target.begin(), target.end(), f.begin() + 4);   // Addr1 (DA) = station
    std::copy(bssid.begin(), bssid.end(), f.begin() + 10);    // Addr2 (SA) = AP/BSSID
    std::copy(bssid.begin(), bssid.end(), f.begin() + 16);    // Addr3 (BSSID)
    f[22] = 0x00;  // sequence control
    f[23] = 0x00;
    f[24] = static_cast<uint8_t>(reason & 0xFF);         // reason code, little-endian
    f[25] = static_cast<uint8_t>((reason >> 8) & 0xFF);
    return f;
}

bool is_zero(const MacAddr& mac)
{
    return std::all_of(mac.begin(), mac.end(), [](uint8_t b) { return b == 0; });
}

}  // namespace

ManagementFrame build_deauth_frame(const MacAddr& bssid, const MacAddr& target, uint16_t reason)
{
    return build_mgmt(0xC0, bssid, target, reason);
}

ManagementFrame build_disassoc_frame(const MacAddr& bssid, const MacAddr& target, uint16_t reason)
{
    return build_mgmt(0xA0, bssid, target, reason);
}

ManagementFrame build_deauth_frame(const DeauthParams& params)
{
    const MacAddr& target = is_zero(params.target) ? kBroadcastMac : params.target;
    return build_deauth_frame(params.bssid, target, params.reason);
}

}  // namespace spectra5::domain

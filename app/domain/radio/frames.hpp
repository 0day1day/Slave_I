#pragma once

#include <array>
#include <cstdint>

#include "domain/radio/offensive.hpp"

// 802.11 management-frame builders for the offensive engine (PRD Phase B).
//
// Pure, hardware-free byte construction so it unit-tests on desktop and is shared
// by the C6 firmware (which builds the frame with these, then transmits it via
// esp_wifi_80211_tx). The injection itself is a C6-firmware concern: it needs the
// libnet80211 sanity-check override + `-zmuldefs` linker flag to allow raw deauth/
// disassoc TX (see docs/architecture/porkchop-port-map.md).
//
// Frame layout ported from M5PORKCHOP `core/wsl_bypasser.cpp` (MIT, (c) 2025 0ct0),
// reimplemented here with a full little-endian 16-bit reason code.

namespace spectra5::domain {

constexpr std::size_t kDeauthFrameSize = 26;
using ManagementFrame = std::array<uint8_t, kDeauthFrameSize>;

// Broadcast destination = deauth/disassoc the whole AP (all clients).
constexpr MacAddr kBroadcastMac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Deauthentication frame (subtype 0xC0). target = station to kick (or broadcast).
ManagementFrame build_deauth_frame(const MacAddr& bssid, const MacAddr& target, uint16_t reason);

// Disassociation frame (subtype 0xA0), same structure.
ManagementFrame build_disassoc_frame(const MacAddr& bssid, const MacAddr& target, uint16_t reason);

// Convenience: build the frame implied by a DeauthParams (broadcast if target is
// all-zero), so the C6 dispatcher can go straight from the command to the bytes.
ManagementFrame build_deauth_frame(const DeauthParams& params);

}  // namespace spectra5::domain

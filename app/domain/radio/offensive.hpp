#pragma once

#include <array>
#include <cstdint>
#include <vector>

// Spectra5 offensive radio protocol (PRD Phase A).
//
// The contract shared by the P4 commander and the ESP32-C6 radio engine. The C6
// runs monitor/injection natively and exchanges high-level intents (commands) and
// compact results (events) with the P4 over the ESP_PRIV_IF side-channel, framed
// by the Phase-5 RPC codec (see rpc.hpp / rpc_codec.hpp). Keeping this header pure
// (no LVGL, no IDF) lets both sides and the desktop tests depend on it.

namespace spectra5::domain {

// Capabilities the C6 engine advertises in the boot handshake. The P4 enables UI
// features from this bitmask, so old/new firmware on either side degrade cleanly.
enum class RadioCapability : uint32_t {
    None        = 0,
    WifiScan    = 1u << 0,  // STA scan (already works over stock esp_hosted)
    Monitor     = 1u << 1,  // promiscuous RX: sniff, pcap, EAPOL/PMKID
    Inject      = 1u << 2,  // raw 802.11 TX
    Deauth      = 1u << 3,
    BeaconSpam  = 1u << 4,
    ProbeSpam   = 1u << 5,
    StationScan = 1u << 6,  // enumerate clients of a BSSID (needs Monitor)
    BleScan     = 1u << 7,
    BleSpam     = 1u << 8,
    Ieee802154  = 1u << 9,
};

inline uint32_t operator|(RadioCapability a, RadioCapability b)
{
    return static_cast<uint32_t>(a) | static_cast<uint32_t>(b);
}

inline uint32_t operator|(uint32_t a, RadioCapability b)
{
    return a | static_cast<uint32_t>(b);
}

inline bool has_capability(uint32_t mask, RadioCapability c)
{
    return (mask & static_cast<uint32_t>(c)) != 0;
}

// Radio role on the C6. Exactly one is active at a time; the C6 arbiter enforces
// it and emits ModeChanged. Networking == the stock esp_hosted STA/AP path.
enum class RadioMode : uint8_t {
    Idle       = 0,
    Networking = 1,
    Monitor    = 2,
    Inject     = 3,
};

// Commands: P4 -> C6.
enum class CommandOpcode : uint8_t {
    Nop             = 0,
    GetCapabilities = 1,
    SetMode         = 2,
    SetChannel      = 3,
    StartMonitor    = 4,
    StopMonitor     = 5,
    ScanStations    = 6,   // enumerate stations on a target BSSID
    StartDeauth     = 7,
    StartBeacon     = 8,
    StartProbe      = 9,
    InjectRaw       = 10,
    Stop            = 11,
    SaeFlood        = 12,  // WPA3 SAE commit-frame flood (router DoS)
    StartKarma      = 13,  // harvest probe-request SSIDs (Karma)
    StartDetect     = 14,  // defensive: alert on deauth/disassoc frames
    StartSniff      = 15,  // live per-type packet statistics
    ZigbeeScan      = 16,  // 802.15.4 energy scan (channels 11-26)
    Ieee154Sniff    = 17,  // 802.15.4 promiscuous frame sniff (param: channel; 0=stop)
};

// Events: C6 -> P4.
enum class EventKind : uint8_t {
    None          = 0,
    Capabilities  = 1,
    ModeChanged   = 2,
    ApSeen        = 3,
    StationSeen   = 4,
    Handshake     = 5,
    Pmkid         = 6,
    FrameCaptured = 7,   // raw frame for live pcap
    AttackStats   = 8,
    Error         = 9,
};

using MacAddr = std::array<uint8_t, 6>;

// ---- Fixed-layout payloads (serialised little-endian by offensive_codec) ----

struct CapabilityReport {
    uint8_t fw_major     = 0;
    uint8_t fw_minor     = 0;
    uint32_t capabilities = 0;  // bitmask of RadioCapability
};

struct DeauthParams {
    MacAddr bssid{};
    MacAddr target{};       // station to kick; all-zero = broadcast (whole AP)
    uint8_t channel = 0;
    uint16_t reason = 7;    // 802.11 reason code (7 = class-3 frame from nonassoc)
    uint16_t bursts = 0;    // frames per round; 0 = continuous
};

struct MonitorParams {
    uint8_t channel      = 0;  // 0 = hop across 2.4 GHz channels
    uint32_t filter_mask = 0;  // EventKind bits the P4 wants forwarded
};

// A station (client) discovered on a target BSSID via monitor mode.
struct Station {
    MacAddr mac{};
    MacAddr bssid{};
    int8_t rssi    = 0;
    uint32_t packets = 0;
};

// Generic envelopes carried in an RpcFrame payload.
struct RadioCommand {
    CommandOpcode opcode = CommandOpcode::Nop;
    std::vector<uint8_t> params;
};

struct RadioEvent {
    EventKind kind = EventKind::None;
    std::vector<uint8_t> data;
};

}  // namespace spectra5::domain

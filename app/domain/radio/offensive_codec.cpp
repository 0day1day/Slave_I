#include "domain/radio/offensive_codec.hpp"

#include <cstring>

namespace spectra5::domain {
namespace {

void put_u8(std::vector<uint8_t>& out, uint8_t v)
{
    out.push_back(v);
}

void put_u16(std::vector<uint8_t>& out, uint16_t v)
{
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

void put_u32(std::vector<uint8_t>& out, uint32_t v)
{
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
    }
}

void put_mac(std::vector<uint8_t>& out, const MacAddr& mac)
{
    out.insert(out.end(), mac.begin(), mac.end());
}

// Cursor-based reader; any over-read flips ok=false so callers bail out cleanly.
struct Reader {
    const std::vector<uint8_t>& buf;
    std::size_t pos = 0;
    bool ok         = true;

    explicit Reader(const std::vector<uint8_t>& b) : buf(b) {}

    uint8_t u8()
    {
        if (pos + 1 > buf.size()) {
            ok = false;
            return 0;
        }
        return buf[pos++];
    }
    uint16_t u16()
    {
        if (pos + 2 > buf.size()) {
            ok = false;
            return 0;
        }
        uint16_t v = static_cast<uint16_t>(buf[pos]) | static_cast<uint16_t>(buf[pos + 1] << 8);
        pos += 2;
        return v;
    }
    uint32_t u32()
    {
        if (pos + 4 > buf.size()) {
            ok = false;
            return 0;
        }
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            v |= static_cast<uint32_t>(buf[pos + i]) << (8 * i);
        }
        pos += 4;
        return v;
    }
    MacAddr mac()
    {
        MacAddr m{};
        if (pos + 6 > buf.size()) {
            ok = false;
            return m;
        }
        std::memcpy(m.data(), buf.data() + pos, 6);
        pos += 6;
        return m;
    }
    bool done() const { return ok && pos == buf.size(); }
};

}  // namespace

std::vector<uint8_t> encode_capability_report(const CapabilityReport& report)
{
    std::vector<uint8_t> out;
    put_u8(out, report.fw_major);
    put_u8(out, report.fw_minor);
    put_u32(out, report.capabilities);
    return out;
}

std::optional<CapabilityReport> decode_capability_report(const std::vector<uint8_t>& data)
{
    Reader r(data);
    CapabilityReport report;
    report.fw_major     = r.u8();
    report.fw_minor     = r.u8();
    report.capabilities = r.u32();
    if (!r.done()) {
        return std::nullopt;
    }
    return report;
}

std::vector<uint8_t> encode_deauth_params(const DeauthParams& params)
{
    std::vector<uint8_t> out;
    put_mac(out, params.bssid);
    put_mac(out, params.target);
    put_u8(out, params.channel);
    put_u16(out, params.reason);
    put_u16(out, params.bursts);
    return out;
}

std::optional<DeauthParams> decode_deauth_params(const std::vector<uint8_t>& data)
{
    Reader r(data);
    DeauthParams params;
    params.bssid   = r.mac();
    params.target  = r.mac();
    params.channel = r.u8();
    params.reason  = r.u16();
    params.bursts  = r.u16();
    if (!r.done()) {
        return std::nullopt;
    }
    return params;
}

std::vector<uint8_t> encode_monitor_params(const MonitorParams& params)
{
    std::vector<uint8_t> out;
    put_u8(out, params.channel);
    put_u32(out, params.filter_mask);
    return out;
}

std::optional<MonitorParams> decode_monitor_params(const std::vector<uint8_t>& data)
{
    Reader r(data);
    MonitorParams params;
    params.channel     = r.u8();
    params.filter_mask = r.u32();
    if (!r.done()) {
        return std::nullopt;
    }
    return params;
}

std::vector<uint8_t> encode_station(const Station& station)
{
    std::vector<uint8_t> out;
    put_mac(out, station.mac);
    put_mac(out, station.bssid);
    put_u8(out, static_cast<uint8_t>(station.rssi));
    put_u32(out, station.packets);
    return out;
}

std::optional<Station> decode_station(const std::vector<uint8_t>& data)
{
    Reader r(data);
    Station station;
    station.mac     = r.mac();
    station.bssid   = r.mac();
    station.rssi    = static_cast<int8_t>(r.u8());
    station.packets = r.u32();
    if (!r.done()) {
        return std::nullopt;
    }
    return station;
}

RpcFrame make_command_frame(uint32_t sequence, const RadioCommand& command)
{
    RpcFrame frame;
    frame.header.message_type = static_cast<uint8_t>(RpcMessageType::Command);
    frame.header.sequence     = sequence;
    frame.payload.reserve(command.params.size() + 1);
    frame.payload.push_back(static_cast<uint8_t>(command.opcode));
    frame.payload.insert(frame.payload.end(), command.params.begin(), command.params.end());
    return frame;
}

std::optional<RadioCommand> command_from_frame(const RpcFrame& frame)
{
    if (frame.header.message_type != static_cast<uint8_t>(RpcMessageType::Command) ||
        frame.payload.empty()) {
        return std::nullopt;
    }
    RadioCommand command;
    command.opcode = static_cast<CommandOpcode>(frame.payload[0]);
    command.params.assign(frame.payload.begin() + 1, frame.payload.end());
    return command;
}

RpcFrame make_event_frame(uint32_t sequence, const RadioEvent& event)
{
    RpcFrame frame;
    frame.header.message_type = static_cast<uint8_t>(RpcMessageType::Event);
    frame.header.sequence     = sequence;
    frame.payload.reserve(event.data.size() + 1);
    frame.payload.push_back(static_cast<uint8_t>(event.kind));
    frame.payload.insert(frame.payload.end(), event.data.begin(), event.data.end());
    return frame;
}

std::optional<RadioEvent> event_from_frame(const RpcFrame& frame)
{
    if (frame.header.message_type != static_cast<uint8_t>(RpcMessageType::Event) ||
        frame.payload.empty()) {
        return std::nullopt;
    }
    RadioEvent event;
    event.kind = static_cast<EventKind>(frame.payload[0]);
    event.data.assign(frame.payload.begin() + 1, frame.payload.end());
    return event;
}

// ---- Typed command builders ----

RadioCommand cmd_get_capabilities()
{
    return {CommandOpcode::GetCapabilities, {}};
}

RadioCommand cmd_wifi_scan()
{
    return {CommandOpcode::Nop, {}};  // scan stays on the existing scanner path for now
}

RadioCommand cmd_ble_scan()
{
    return {CommandOpcode::Nop, {}};
}

RadioCommand cmd_stop()
{
    return {CommandOpcode::Stop, {}};
}

RadioCommand cmd_set_channel(uint8_t channel)
{
    return {CommandOpcode::SetChannel, {channel}};
}

RadioCommand cmd_scan_stations(const MacAddr& bssid, uint8_t channel)
{
    RadioCommand cmd{CommandOpcode::ScanStations, {}};
    cmd.params.insert(cmd.params.end(), bssid.begin(), bssid.end());
    cmd.params.push_back(channel);
    return cmd;
}

RadioCommand cmd_deauth(const DeauthParams& params)
{
    return {CommandOpcode::StartDeauth, encode_deauth_params(params)};
}

RadioCommand cmd_start_monitor(const MonitorParams& params)
{
    return {CommandOpcode::StartMonitor, encode_monitor_params(params)};
}

RadioCommand cmd_beacon(const std::vector<std::string>& ssids, uint8_t channel)
{
    // Wire: channel(1) count(1) then count x [ ssid_len(1) ssid bytes ]. The C6
    // emits all SSIDs on `channel`; the P4 rotates the channel across 1/6/11.
    RadioCommand cmd{CommandOpcode::StartBeacon, {}};
    const uint8_t count = static_cast<uint8_t>(ssids.size() > 16 ? 16 : ssids.size());
    cmd.params.push_back(channel);
    cmd.params.push_back(count);
    for (uint8_t i = 0; i < count; ++i) {
        const std::string& s = ssids[i];
        const uint8_t len    = static_cast<uint8_t>(s.size() > 32 ? 32 : s.size());
        cmd.params.push_back(len);
        cmd.params.insert(cmd.params.end(), s.begin(), s.begin() + len);
    }
    return cmd;
}

RadioCommand cmd_sae_flood(const MacAddr& bssid, uint8_t channel)
{
    // Wire: bssid(6) channel(1). The C6 floods SAE commit auth frames from random
    // source MACs to exhaust the AP's SAE state table (WPA3 DoS).
    RadioCommand cmd{CommandOpcode::SaeFlood, {}};
    cmd.params.insert(cmd.params.end(), bssid.begin(), bssid.end());
    cmd.params.push_back(channel);
    return cmd;
}

RadioCommand cmd_start_karma(uint8_t channel)
{
    return {CommandOpcode::StartKarma, {channel}};
}

RadioCommand cmd_start_detect()
{
    return {CommandOpcode::StartDetect, {}};
}

RadioCommand cmd_start_sniff(uint8_t channel)
{
    return {CommandOpcode::StartSniff, {channel}};
}

RadioCommand cmd_zigbee_scan()
{
    return {CommandOpcode::ZigbeeScan, {}};
}

RadioCommand cmd_154_sniff(uint8_t channel)
{
    return {CommandOpcode::Ieee154Sniff, {channel}};
}

// ---- Typed event builders ----

RadioEvent evt_capabilities(const CapabilityReport& report)
{
    return {EventKind::Capabilities, encode_capability_report(report)};
}

RadioEvent evt_station_seen(const Station& station)
{
    return {EventKind::StationSeen, encode_station(station)};
}

RadioEvent evt_mode_changed(RadioMode mode)
{
    return {EventKind::ModeChanged, {static_cast<uint8_t>(mode)}};
}

}  // namespace spectra5::domain

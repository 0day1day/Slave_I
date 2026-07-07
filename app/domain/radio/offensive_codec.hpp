#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "domain/radio/offensive.hpp"
#include "domain/radio/rpc.hpp"

// Serialisation for the offensive radio protocol (PRD Phase A). Payload structs
// are encoded little-endian with a fixed layout so the P4 (host, little-endian
// RISC-V) and the C6 (also little-endian) agree byte-for-byte. Command/event
// envelopes wrap into an RpcFrame for transport over the ESP_PRIV_IF side-channel.

namespace spectra5::domain {

// ---- Typed payloads ----
std::vector<uint8_t> encode_capability_report(const CapabilityReport& report);
std::optional<CapabilityReport> decode_capability_report(const std::vector<uint8_t>& data);

std::vector<uint8_t> encode_deauth_params(const DeauthParams& params);
std::optional<DeauthParams> decode_deauth_params(const std::vector<uint8_t>& data);

std::vector<uint8_t> encode_monitor_params(const MonitorParams& params);
std::optional<MonitorParams> decode_monitor_params(const std::vector<uint8_t>& data);

std::vector<uint8_t> encode_station(const Station& station);
std::optional<Station> decode_station(const std::vector<uint8_t>& data);

// ---- Command / event envelopes (carried in an RpcFrame payload) ----
RpcFrame make_command_frame(uint32_t sequence, const RadioCommand& command);
std::optional<RadioCommand> command_from_frame(const RpcFrame& frame);

RpcFrame make_event_frame(uint32_t sequence, const RadioEvent& event);
std::optional<RadioEvent> event_from_frame(const RpcFrame& frame);

// ---- Typed command builders (the UI/RadioEngine intents -> a RadioCommand) ----
// One per "P4 tells the C6: do X". Params are encoded by the helpers above so the
// UI never hand-packs bytes.
RadioCommand cmd_get_capabilities();
RadioCommand cmd_wifi_scan();
RadioCommand cmd_ble_scan();
RadioCommand cmd_stop();
RadioCommand cmd_set_channel(uint8_t channel);
RadioCommand cmd_scan_stations(const MacAddr& bssid, uint8_t channel);
RadioCommand cmd_deauth(const DeauthParams& params);
RadioCommand cmd_start_monitor(const MonitorParams& params);
RadioCommand cmd_beacon(const std::vector<std::string>& ssids, uint8_t channel);
RadioCommand cmd_sae_flood(const MacAddr& bssid, uint8_t channel);
RadioCommand cmd_start_karma(uint8_t channel);
RadioCommand cmd_start_detect();
RadioCommand cmd_start_sniff(uint8_t channel);  // channel 0 = current
RadioCommand cmd_zigbee_scan();
RadioCommand cmd_154_sniff(uint8_t channel);    // 802.15.4 sniff; channel 0 = stop

// ---- Typed event builders (C6 side / mock -> a RadioEvent) ----
RadioEvent evt_capabilities(const CapabilityReport& report);
RadioEvent evt_station_seen(const Station& station);
RadioEvent evt_mode_changed(RadioMode mode);

}  // namespace spectra5::domain

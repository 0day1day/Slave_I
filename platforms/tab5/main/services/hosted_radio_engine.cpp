#include "services/hosted_radio_engine.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

extern "C" {
#include "esp_hosted_api.h"
}

#include "core/diagnostics/log.hpp"
#include "services/radio/capture_store.hpp"
#include "services/radio/deauth_detector.hpp"
#include "services/radio/ieee154_store.hpp"
#include "services/radio/pcap_store.hpp"
#include "services/radio/probe_store.hpp"
#include "services/radio/sniffer_store.hpp"
#include "services/radio/radio_console.hpp"
#include "services/radio/station_store.hpp"
#include "services/radio/zigbee_store.hpp"
#include "services/storage/sd_logger.hpp"

namespace spectra5::platform {

using namespace spectra5::domain;

namespace {
// Human-readable console line for a command about to go to the C6.
std::string describe(const RadioCommand& cmd)
{
    char line[96];
    if (cmd.opcode == CommandOpcode::StartDeauth && cmd.params.size() >= 13) {
        const auto& p = cmd.params;
        std::snprintf(line, sizeof(line),
                      "> DEAUTH  ch %u  %02x:%02x:%02x:%02x:%02x:%02x", p[12], p[0], p[1], p[2],
                      p[3], p[4], p[5]);
    } else {
        std::snprintf(line, sizeof(line), "> cmd opcode=%d (%d bytes)",
                      static_cast<int>(cmd.opcode), static_cast<int>(cmd.params.size()));
    }
    return line;
}
}  // namespace

uint32_t HostedRadioEngine::capabilities() const
{
    // Hardcoded to what the current C6 firmware implements (deauth injection,
    // channel control, promiscuous station discovery). Replace with the value
    // reported by the C6 boot handshake once that lands.
    return RadioCapability::Inject | RadioCapability::Deauth | RadioCapability::Monitor |
           RadioCapability::StationScan | RadioCapability::BeaconSpam;
}

void HostedRadioEngine::set_event_handler(EventHandler handler)
{
    handler_ = std::move(handler);  // C6 -> P4 event path is a follow-up
}

bool HostedRadioEngine::send(const RadioCommand& command)
{
    std::vector<uint8_t> buf;
    buf.reserve(command.params.size() + 1);
    buf.push_back(static_cast<uint8_t>(command.opcode));
    buf.insert(buf.end(), command.params.begin(), command.params.end());
    if (buf.empty() || buf.size() > 255) {
        return false;
    }

    const esp_err_t err =
        esp_hosted_send_priv_command(buf.data(), static_cast<uint8_t>(buf.size()));
    if (err != ESP_OK) {
        spectra5::log::tagError("radio-engine", "send_priv_command failed: {}",
                                esp_err_to_name(err));
        return false;
    }
    spectra5::log::tagInfo("radio-engine", "command opcode={} ({} bytes) -> C6",
                           static_cast<int>(command.opcode), static_cast<int>(buf.size()));
    services::RadioConsole::instance().log(describe(command));
    return true;
}

}  // namespace spectra5::platform

// Strong definition of the weak hook in esp_hosted's transport: a C6 result
// arrives here (on the esp_hosted RX task) and is surfaced in the UI console.
extern "C" void spectra5_on_c6_result(uint8_t opcode, int8_t rc)
{
    char line[80];
    if (opcode == static_cast<uint8_t>(spectra5::domain::CommandOpcode::StartDeauth)) {
        if (rc >= 0) {
            std::snprintf(line, sizeof(line), "< C6: deauth sent %d frames", rc);
        } else {
            std::snprintf(line, sizeof(line), "< C6: deauth FAILED (80211_tx err)");
        }
    } else {
        std::snprintf(line, sizeof(line), "< C6: opcode %u rc=%d", opcode, rc);
    }
    spectra5::services::RadioConsole::instance().log(line);
}

// A station (client) discovered by the C6's promiscuous monitor.
extern "C" void spectra5_on_c6_station(const uint8_t* mac, const uint8_t* bssid, int8_t rssi)
{
    spectra5::domain::Station s;
    std::copy(mac, mac + 6, s.mac.begin());
    std::copy(bssid, bssid + 6, s.bssid.begin());
    s.rssi = rssi;
    spectra5::services::StationStore::instance().add(s);

    char line[80];
    std::snprintf(line, sizeof(line), "  STA %02x:%02x:%02x:%02x:%02x:%02x   %d dBm", mac[0], mac[1],
                  mac[2], mac[3], mac[4], mac[5], rssi);
    spectra5::services::RadioConsole::instance().log(line);
}

namespace {
std::string hex_bytes(const uint8_t* d, std::size_t n)
{
    static const char* H = "0123456789abcdef";
    std::string s;
    s.reserve(n * 2);
    for (std::size_t i = 0; i < n; ++i) {
        s.push_back(H[d[i] >> 4]);
        s.push_back(H[d[i] & 0x0F]);
    }
    return s;
}
}  // namespace

// A probe-request SSID harvested by the C6 Karma capture (a network some nearby
// client is looking for). Collected so the Karma responder can beacon it back.
extern "C" void spectra5_on_c6_probe(const uint8_t* ssid, uint8_t len)
{
    std::string s(reinterpret_cast<const char*>(ssid), len);
    spectra5::services::ProbeStore::instance().add(s);
    spectra5::services::RadioConsole::instance().log(std::string("  PROBE \"") + s + "\"");
    spectra5::services::SdLogger::instance().enqueue("wifi/probes.txt", s);
}

// A deauth/disassoc frame the C6 detected (defensive detector). src = transmitter.
extern "C" void spectra5_on_c6_deauth(const uint8_t* src)
{
    char mac[20];
    std::snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x", src[0], src[1], src[2], src[3],
                  src[4], src[5]);
    spectra5::services::DeauthDetector::instance().report(mac);
    spectra5::services::RadioConsole::instance().log(std::string("! DEAUTH from ") + mac);
    spectra5::services::SdLogger::instance().enqueue("wifi/deauth.log", std::string("deauth ") + mac);
}

// 802.15.4 energy scan result from the C6 (16 x int8 peak RSSI, channels 11..26).
extern "C" void spectra5_on_c6_zigbee(const int8_t* powers16)
{
    std::array<int8_t, 16> p{};
    for (int i = 0; i < 16; ++i) {
        p[i] = powers16[i];
    }
    spectra5::services::ZigbeeStore::instance().set(p);
    spectra5::services::RadioConsole::instance().log("802.15.4 energy scan complete (ch 11-26)");
}

// Live sniffer per-type frame counts from the C6 (5 x uint32 LE).
extern "C" void spectra5_on_c6_sniff(const uint8_t* counts20)
{
    std::array<uint32_t, 5> c{};
    for (int i = 0; i < 5; ++i) {
        c[i] = static_cast<uint32_t>(counts20[i * 4]) |
               (static_cast<uint32_t>(counts20[i * 4 + 1]) << 8) |
               (static_cast<uint32_t>(counts20[i * 4 + 2]) << 16) |
               (static_cast<uint32_t>(counts20[i * 4 + 3]) << 24);
    }
    spectra5::services::SnifferStore::instance().set(c);
}

// An EAPOL handshake message / PMKID captured by the C6 monitor. kind = M1..M4.
extern "C" void spectra5_on_c6_capture(uint8_t kind, const uint8_t* ap, const uint8_t* client,
                                       const uint8_t* pmkid)
{
    using namespace spectra5::services;
    if (pmkid != nullptr) {
        // hashcat 22000 PMKID line: WPA*01*PMKID*AP*CLIENT*ESSID***
        const std::string essid = CaptureStore::instance().essid();
        std::string hc = "WPA*01*" + hex_bytes(pmkid, 16) + "*" + hex_bytes(ap, 6) + "*" +
                         hex_bytes(client, 6) + "*" +
                         hex_bytes(reinterpret_cast<const uint8_t*>(essid.data()), essid.size()) +
                         "***";
        // Queue only -- the LVGL thread flushes to SD (no file IO from this task).
        CaptureStore::instance().queue_line(hc);
        SdLogger::instance().enqueue("wifi/handshakes.hc22000", hc);
        CaptureStore::instance().on_pmkid();
        char line[96];
        std::snprintf(line, sizeof(line), "*** PMKID CAPTURED %02x:%02x:%02x:%02x:%02x:%02x -> SD",
                      ap[0], ap[1], ap[2], ap[3], ap[4], ap[5]);
        RadioConsole::instance().log(line);
    } else {
        CaptureStore::instance().on_eapol(kind);
        char line[64];
        std::snprintf(line, sizeof(line), "  EAPOL M%u captured", kind);
        RadioConsole::instance().log(line);
    }
}

// Raw 802.11 EAPOL frame forwarded by the C6 (event 0x5D), queued for the .pcap.
extern "C" void spectra5_on_c6_rawframe(const uint8_t* frame, uint8_t len)
{
    spectra5::services::PcapStore::instance().push(frame, len);
}

// Raw 802.15.4 frame forwarded by the C6 (event 0x5E): parse MAC -> device list.
extern "C" void spectra5_on_c6_154frame(uint8_t channel, uint8_t lqi, int8_t rssi,
                                        const uint8_t* frame, uint8_t len)
{
    spectra5::services::Ieee154Store::instance().on_frame(channel, lqi, rssi, frame, len);
}

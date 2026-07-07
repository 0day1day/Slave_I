#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace spectra5::services {

// Accumulates the raw 802.11 EAPOL frames the C6 forwards during a handshake
// capture, so the UI thread can drain them and write a minimal but valid .pcap
// (LINKTYPE_IEEE802_11) next to the hashcat-22000 file. The C6 cannot touch the
// microSD (it's wired to the P4), so frames come over the esp_hosted priv channel
// (event 0x5D) -- finite, one event per EAPOL frame, never full-air streaming.
class PcapStore {
public:
    static PcapStore& instance();

    // Called from the hosted RX task -> just enqueue (no file IO off the UI thread).
    void push(const uint8_t* frame, std::uint8_t len);

    // Called from the UI thread; returns and clears the queued frames.
    std::vector<std::vector<std::uint8_t>> drain();

    void set_essid(const std::string& essid);
    std::string essid();

private:
    PcapStore() = default;

    std::mutex mutex_;
    std::vector<std::vector<std::uint8_t>> frames_;
    std::string essid_;
};

}  // namespace spectra5::services

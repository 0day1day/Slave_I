#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <vector>

namespace spectra5::services {

// A device discovered by the 802.15.4 sniffer (parsed from a frame's MAC header,
// which is in cleartext even when the payload is encrypted).
struct Ieee154Device {
    std::uint16_t pan        = 0xFFFF;
    std::uint16_t short_addr = 0xFFFE;        // 0xFFFE if the device used its EUI-64
    std::array<std::uint8_t, 8> ext_addr{};   // EUI-64 in canonical (big-endian) order
    bool          has_ext    = false;
    std::uint8_t  frame_type = 0;             // last MAC frame type (0 beacon,1 data,3 cmd)
    std::int8_t   rssi       = 0;
    std::uint8_t  lqi        = 0;
    std::uint8_t  channel    = 0;
    std::uint32_t count      = 0;
};

// Parses raw 802.15.4 frames the C6 forwards: dedups senders into a device list
// (PAN / address / frame type / LQI) and queues the raw frames for a .pcap.
class Ieee154Store {
public:
    static Ieee154Store& instance();

    // From the hosted RX task: parse the MAC header + dedup + queue the raw frame.
    void on_frame(std::uint8_t channel, std::uint8_t lqi, std::int8_t rssi,
                  const std::uint8_t* frame, std::uint8_t len);

    std::vector<Ieee154Device> devices();
    std::uint32_t revision();

    // UI thread: returns and clears the raw frames queued for the .pcap.
    std::vector<std::vector<std::uint8_t>> drain_frames();

    void clear();

private:
    Ieee154Store() = default;

    std::mutex mutex_;
    std::vector<Ieee154Device> devices_;
    std::vector<std::vector<std::uint8_t>> frames_;
    std::uint32_t revision_ = 0;
};

}  // namespace spectra5::services

#include "services/radio/pcap_store.hpp"

namespace spectra5::services {

PcapStore& PcapStore::instance()
{
    static PcapStore store;
    return store;
}

void PcapStore::push(const uint8_t* frame, std::uint8_t len)
{
    if (frame == nullptr || len == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (frames_.size() >= 64) {
        return;  // bound memory; a handshake is only a handful of frames
    }
    frames_.emplace_back(frame, frame + len);
}

std::vector<std::vector<std::uint8_t>> PcapStore::drain()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::vector<std::uint8_t>> out;
    out.swap(frames_);
    return out;
}

void PcapStore::set_essid(const std::string& essid)
{
    std::lock_guard<std::mutex> lock(mutex_);
    essid_ = essid;
}

std::string PcapStore::essid()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return essid_;
}

}  // namespace spectra5::services

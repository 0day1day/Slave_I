#include "services/radio/ieee154_store.hpp"

namespace spectra5::services {

Ieee154Store& Ieee154Store::instance()
{
    static Ieee154Store store;
    return store;
}

void Ieee154Store::on_frame(std::uint8_t channel, std::uint8_t lqi, std::int8_t rssi,
                            const std::uint8_t* frame, std::uint8_t len)
{
    if (frame == nullptr || len < 3) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Queue the raw frame for the .pcap (bounded).
    if (frames_.size() < 64) {
        frames_.emplace_back(frame, frame + len);
    }

    // Parse the MAC header (all little-endian on the wire, cleartext).
    const std::uint16_t fcf = static_cast<std::uint16_t>(frame[0] | (frame[1] << 8));
    const std::uint8_t ftype       = fcf & 0x07;
    const bool          pan_compress = (fcf >> 6) & 0x01;
    const std::uint8_t dest_mode   = (fcf >> 10) & 0x03;  // 0 none, 2 short, 3 ext
    const std::uint8_t src_mode    = (fcf >> 14) & 0x03;

    int off = 3;  // FCF(2) + seq(1)
    std::uint16_t dest_pan = 0xFFFF;
    if (dest_mode != 0) {
        if (off + 2 > len) return;
        dest_pan = static_cast<std::uint16_t>(frame[off] | (frame[off + 1] << 8));
        off += 2;
        off += (dest_mode == 2) ? 2 : 8;  // skip dest address
    }
    if (off > len) return;

    std::uint16_t src_pan = dest_pan;
    if (src_mode != 0) {
        if (!pan_compress) {
            if (off + 2 > len) return;
            src_pan = static_cast<std::uint16_t>(frame[off] | (frame[off + 1] << 8));
            off += 2;
        }
    }

    Ieee154Device dev;
    dev.pan        = (src_mode != 0) ? src_pan : dest_pan;
    dev.frame_type = ftype;
    dev.rssi       = rssi;
    dev.lqi        = lqi;
    dev.channel    = channel;

    if (src_mode == 2) {  // short source address
        if (off + 2 > len) return;
        dev.short_addr = static_cast<std::uint16_t>(frame[off] | (frame[off + 1] << 8));
    } else if (src_mode == 3) {  // extended (EUI-64) source address
        if (off + 8 > len) return;
        // Wire is little-endian; store canonical (big-endian) for display + OUI.
        for (int i = 0; i < 8; ++i) {
            dev.ext_addr[i] = frame[off + 7 - i];
        }
        dev.has_ext = true;
    } else {
        return;  // no source address -> nothing to attribute
    }

    // Dedup: same PAN + same address (short or ext).
    for (auto& d : devices_) {
        const bool same = (d.has_ext == dev.has_ext) && d.pan == dev.pan &&
                          (dev.has_ext ? (d.ext_addr == dev.ext_addr)
                                       : (d.short_addr == dev.short_addr));
        if (same) {
            d.frame_type = ftype;
            d.rssi       = rssi;
            d.lqi        = lqi;
            d.channel    = channel;
            ++d.count;
            ++revision_;
            return;
        }
    }
    dev.count = 1;
    if (devices_.size() < 64) {
        devices_.push_back(dev);
        ++revision_;
    }
}

std::vector<Ieee154Device> Ieee154Store::devices()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return devices_;
}

std::uint32_t Ieee154Store::revision()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return revision_;
}

std::vector<std::vector<std::uint8_t>> Ieee154Store::drain_frames()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::vector<std::uint8_t>> out;
    out.swap(frames_);
    return out;
}

void Ieee154Store::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    devices_.clear();
    frames_.clear();
    ++revision_;
}

}  // namespace spectra5::services

#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace spectra5::services {

// Tracks EAPOL/PMKID capture progress for the current monitor target, so the UI
// can show "N PMKIDs / M handshake msgs captured". The hashcat-22000 lines are
// written to the microSD by the capture hook; this just holds counts + the ESSID
// (needed to format the PMKID line). Written from the esp_hosted RX task.
class CaptureStore {
public:
    static CaptureStore& instance();

    void set_target(const std::string& essid);  // called when a client scan starts
    std::string essid() const;

    void on_pmkid();
    void on_eapol(uint8_t msg);  // msg 1..4

    // hashcat lines are queued from the (small-stack) RX task and flushed to the
    // microSD from the LVGL thread -- never do file IO from the capture hook.
    void queue_line(const std::string& line);
    std::vector<std::string> drain_lines();

    int pmkid_count() const;
    int eapol_count() const;
    uint8_t last_msg() const;
    uint32_t revision() const;

private:
    CaptureStore() = default;

    mutable std::mutex mutex_;
    std::string essid_;
    int pmkid_count_ = 0;
    int eapol_count_ = 0;
    uint8_t last_msg_ = 0;
    uint32_t revision_ = 0;
    std::vector<std::string> pending_lines_;
};

}  // namespace spectra5::services

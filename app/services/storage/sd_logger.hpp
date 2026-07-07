#pragma once

#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace spectra5::services {

// Structured logging to the microSD. Capture hooks run on tasks that must NOT do
// file IO (the esp_hosted RX task, the httpd task) -- they enqueue() lines (cheap,
// thread-safe) and the UI thread calls flush() to write them under /sd/spectra5/<rel>.
//
// Layout:
//   /sd/spectra5/wifi/scan.csv          AP scans (on demand)
//   /sd/spectra5/wifi/creds.txt         captured credentials
//   /sd/spectra5/wifi/handshakes.hc22000  PMKID/EAPOL (hashcat 22000)
//   /sd/spectra5/wifi/probes.txt        Karma harvested SSIDs
//   /sd/spectra5/wifi/deauth.log        deauth-detector alerts
//   /sd/spectra5/ble/scan.csv           BLE scans (on demand)
//   /sd/spectra5/events.log             general events
class SdLogger {
public:
    static SdLogger& instance();

    // Create the directory tree (idempotent). Call once from the UI thread at boot.
    void ensure_dirs();

    // Queue a line for `relpath` (under /sd/spectra5/). Safe from any task.
    void enqueue(const std::string& relpath, std::string line);

    // Write all queued lines to their files. Call from the UI thread only.
    void flush();

    std::size_t pending() const;

private:
    SdLogger() = default;
    mutable std::mutex mutex_;
    std::vector<std::pair<std::string, std::string>> queue_;
    bool dirs_ready_ = false;
};

}  // namespace spectra5::services

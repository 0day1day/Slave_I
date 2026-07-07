#pragma once

#include <mutex>
#include <vector>

#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "core/clock.hpp"
#include "services/radio/wifi_scanner.hpp"

namespace spectra5::platform {

// Real Wi-Fi scanner for the M5Stack Tab5. All esp_wifi_* calls run on a single
// dedicated FreeRTOS task so esp_wifi_remote (P4↔C6 RPC) is never invoked from
// the LVGL/input thread — that race caused the blue-screen reboots.
class Esp32WifiScanner final : public services::IWifiScanner {
public:
    explicit Esp32WifiScanner(IClock& clock);
    ~Esp32WifiScanner() override;

    void start() override;
    void stop() override;
    void release_radio() override;
    bool is_scanning() const override;
    bool is_radio_idle() const override;
    std::vector<domain::WifiAccessPoint> snapshot() override;

private:
    enum class Cmd : uint8_t { Wake, ReleaseRadio };

    bool ensure_task();
    bool ensure_initialised();
    void tear_down_wifi_stack();
    void scan_loop();
    static void scan_task_trampoline(void* arg);
    static void on_scan_done(void* arg, esp_event_base_t base, int32_t id, void* data);

    IClock& clock_;
    mutable std::mutex mutex_;
    std::vector<domain::WifiAccessPoint> aps_;
    bool initialised_      = false;
    bool scanning_         = false;
    bool release_pending_  = false;
    bool shutdown_         = false;
    TaskHandle_t task_     = nullptr;
    QueueHandle_t cmd_queue_   = nullptr;
    EventGroupHandle_t events_ = nullptr;
    esp_event_handler_instance_t scan_done_inst_ = nullptr;  // SCAN_DONE handler, unregistered on teardown
};

}  // namespace spectra5::platform

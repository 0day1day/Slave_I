#pragma once

#include <mutex>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "core/clock.hpp"
#include "services/radio/ble_scanner.hpp"
#include "services/radio/ble_spammer.hpp"

struct ble_gap_event;  // NimBLE (global scope) -- full type via host/ble_gap.h in the .cpp

namespace spectra5::platform {

// Nimble BLE scanner for Tab5 (P4 host + C6 controller via esp_hosted VHCI).
// All Nimble init and GAP calls run on dedicated tasks — never from LVGL.
class Esp32BleScanner final : public services::IBleScanner, public services::IBleSpammer {
public:
    explicit Esp32BleScanner(IClock& clock);
    ~Esp32BleScanner() override;

    void start() override;
    void stop() override;
    void release_radio() override;
    bool is_scanning() const override;
    bool is_radio_idle() const override;
    std::vector<domain::BleAdvertisement> snapshot() override;

    // IBleSpammer: cycle pairing-popup advertisements (mutually exclusive with scan).
    void start_spam() override;
    void stop_spam() override;
    bool is_spamming() const override;
    void set_spam_mode(int mode) override;

    void on_host_sync();
    void on_disc(const void* disc_desc);
    void on_disc_complete();
    bool wants_scan() const;
    void apply_host_state();

private:
    enum class Cmd : uint8_t { Wake, ReleaseRadio };

    bool ensure_ctrl_task();
    void ctrl_loop();
    void shutdown_radio_stack();
    bool init_nimble_stack();
    void schedule_host_work();
    void begin_scan();
    void cancel_scan();
    void begin_spam();
    void stop_adv();
    static int spam_gap_event(struct ble_gap_event* event, void* arg);

    static void ctrl_task_trampoline(void* arg);
    static void host_task(void* arg);
    static void on_reset(int reason);
    static void on_sync();

    IClock& clock_;
    mutable std::mutex mutex_;
    std::vector<domain::BleAdvertisement> devices_;
    bool nimble_inited_    = false;
    bool host_running_     = false;
    bool host_synced_      = false;
    bool scanning_         = false;
    bool disc_active_      = false;
    bool spam_             = false;
    bool adv_active_       = false;
    int spam_mode_         = 0;  // 0 all, 1 Apple, 2 Samsung, 3 Google, 4 Microsoft
    bool release_pending_  = false;
    bool shutdown_         = false;
    TaskHandle_t ctrl_task_      = nullptr;
    QueueHandle_t cmd_queue_     = nullptr;
    EventGroupHandle_t events_   = nullptr;
};

}  // namespace spectra5::platform

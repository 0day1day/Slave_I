#pragma once

#include <string>

#include "services/radio/evil_portal_service.hpp"

namespace spectra5::platform {

// Evil Portal: brings up an open SoftAP on the C6, a DNS hijack (every lookup ->
// us) and an HTTP captive portal that harvests whatever credentials a victim
// submits. Works regardless of PMF/band because it's OUR access point. While
// active it owns the C6 radio (the STA scanner is stopped); stop() restores it.
//
// Captured credentials go to the RadioConsole and are queued to the microSD
// (/sd/spectra5/credentials.txt) via CaptureStore.
class EvilPortal final : public services::IEvilPortal {
public:
    static EvilPortal& instance();

    bool start(const std::string& ssid) override;
    void stop() override;
    bool active() const override;
    int captured() const override;  // number of credential submissions
    void note_capture();            // called by the HTTP POST handler

    void verify_last() override;
    int verify_state() const override;
    std::string last_password() const override;
    void set_last_password(const std::string& pw);  // called by the HTTP POST handler
    void set_ap_channel(uint8_t ch) override;
    void set_template(const std::string& path) override;
    void set_template_html(const std::string& html) override;

private:
    EvilPortal() = default;

    void ensure_worker();
    void do_start();
    void do_stop();
    void do_verify();

    volatile bool desired_     = false;  // requested state (set by start/stop)
    volatile bool active_      = false;  // actual state (owned by the worker)
    volatile bool verify_req_  = false;  // a verify was requested
    volatile int verify_state_ = 0;      // 0 idle, 1 verifying, 2 valid, 3 invalid
    int captured_              = 0;
    uint8_t ap_channel_       = 1;       // SoftAP channel (match real AP for deauth+twin)
    std::string pending_ssid_;
    std::string last_password_;
    std::string template_path_;          // SD HTML template, empty = built-in page
    std::string template_html_;          // inline (embedded) HTML, overrides file/built-in
    void* worker_      = nullptr;  // TaskHandle_t (reconciles desired -> active)
    void* http_handle_ = nullptr;  // httpd_handle_t
    void* ap_netif_    = nullptr;  // esp_netif_t*
    void* dns_task_    = nullptr;  // TaskHandle_t

    friend void evil_portal_dns_task(void*);
    friend void evil_portal_worker(void*);
};

}  // namespace spectra5::platform

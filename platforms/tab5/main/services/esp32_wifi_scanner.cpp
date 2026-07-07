#include "services/esp32_wifi_scanner.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include "core/diagnostics/log.hpp"
#include "services/radio/radio_coordinator.hpp"

namespace spectra5::platform {

using namespace spectra5::domain;

namespace {

constexpr const char* kTag         = "wifi-scan";
constexpr std::size_t kHistoryMax  = 48;
constexpr EventBits_t kScanDoneBit = BIT0;
constexpr EventBits_t kRadioIdleBit = BIT1;

WifiSecurity map_security(wifi_auth_mode_t mode)
{
    switch (mode) {
        case WIFI_AUTH_OPEN:            return WifiSecurity::Open;
        case WIFI_AUTH_WEP:             return WifiSecurity::Wep;
        case WIFI_AUTH_WPA_PSK:         return WifiSecurity::Wpa;
        case WIFI_AUTH_WPA2_PSK:
        case WIFI_AUTH_WPA_WPA2_PSK:    return WifiSecurity::Wpa2;
        case WIFI_AUTH_WPA3_PSK:
        case WIFI_AUTH_WPA2_WPA3_PSK:   return WifiSecurity::Wpa3;
        case WIFI_AUTH_WPA2_ENTERPRISE: return WifiSecurity::WpaEnterprise;
        default:                        return WifiSecurity::Wpa2;
    }
}

std::string format_bssid(const uint8_t bssid[6])
{
    char buf[18];
    std::snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1], bssid[2],
                  bssid[3], bssid[4], bssid[5]);
    return buf;
}

}  // namespace

Esp32WifiScanner::Esp32WifiScanner(IClock& clock) : clock_(clock)
{
    events_    = xEventGroupCreate();
    cmd_queue_ = xQueueCreate(8, sizeof(Cmd));
    if (events_ != nullptr) {
        xEventGroupSetBits(events_, kRadioIdleBit);
    }
}

Esp32WifiScanner::~Esp32WifiScanner()
{
    release_radio();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
    }
    if (cmd_queue_ != nullptr) {
        const Cmd wake = Cmd::Wake;
        xQueueSend(cmd_queue_, &wake, 0);
    }
    for (int i = 0; i < 80 && task_ != nullptr; ++i) {
        vTaskDelay(pdMS_TO_TICKS(25));
    }
    if (events_ != nullptr) {
        vEventGroupDelete(events_);
        events_ = nullptr;
    }
    if (cmd_queue_ != nullptr) {
        vQueueDelete(cmd_queue_);
        cmd_queue_ = nullptr;
    }
}

void Esp32WifiScanner::on_scan_done(void* arg, esp_event_base_t, int32_t, void*)
{
    auto* self = static_cast<Esp32WifiScanner*>(arg);
    if (self != nullptr && self->events_ != nullptr) {
        xEventGroupSetBits(self->events_, kScanDoneBit);
    }
}

void Esp32WifiScanner::tear_down_wifi_stack()
{
    if (initialised_) {
        esp_wifi_scan_stop();
        esp_wifi_stop();
        if (scan_done_inst_ != nullptr) {
            esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, scan_done_inst_);
            scan_done_inst_ = nullptr;
        }
        esp_wifi_deinit();
        initialised_ = false;
        spectra5::log::tagInfo(kTag, "Wi-Fi stack released on C6");
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        release_pending_ = false;
    }
    if (events_ != nullptr) {
        xEventGroupSetBits(events_, kRadioIdleBit);
    }
}

bool Esp32WifiScanner::ensure_initialised()
{
    if (initialised_) {
        return true;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        spectra5::log::tagError(kTag, "nvs_flash_init failed: {}", esp_err_to_name(ret));
        return false;
    }

    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        spectra5::log::tagError(kTag, "esp_netif_init failed: {}", esp_err_to_name(ret));
        return false;
    }
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        spectra5::log::tagError(kTag, "event loop create failed: {}", esp_err_to_name(ret));
        return false;
    }

    // No netif on purpose: scanning (and later monitor/inject) is pure radio RPC
    // and never needs an IP stack. Creating the default STA netif here made the
    // acquire/release cycle (Wi-Fi<->BLE handoff) abort with "netif already
    // added" -- the default STA_START/STOP handlers add/remove the lwIP netif
    // asynchronously over esp_wifi_remote, so re-init re-adds it before the prior
    // remove lands. A feature that genuinely needs IP (e.g. Evil Portal SoftAP)
    // will own its netif in its own module.

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret                    = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        spectra5::log::tagError(kTag, "esp_wifi_init failed: {}", esp_err_to_name(ret));
        return false;
    }

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        spectra5::log::tagError(kTag, "set_mode STA failed: {}", esp_err_to_name(ret));
        return false;
    }
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        spectra5::log::tagError(kTag, "esp_wifi_start failed: {}", esp_err_to_name(ret));
        return false;
    }

    esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &Esp32WifiScanner::on_scan_done,
                                        this, &scan_done_inst_);

    initialised_ = true;
    if (events_ != nullptr) {
        xEventGroupClearBits(events_, kRadioIdleBit);
    }
    spectra5::log::tagInfo(kTag, "real Wi-Fi (C6) STA initialised");
    return true;
}

bool Esp32WifiScanner::ensure_task()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (task_ != nullptr) {
        return true;
    }
    if (xTaskCreate(&Esp32WifiScanner::scan_task_trampoline, "wifi_scan", 8192, this, 4, &task_) != pdPASS) {
        spectra5::log::tagError(kTag, "failed to create scan task");
        return false;
    }
    return true;
}

void Esp32WifiScanner::start()
{
    if (auto* coordinator = services::radio_coordinator()) {
        if (!coordinator->acquire_for_wifi()) {
            spectra5::log::tagError(kTag, "could not acquire C6 radio for Wi-Fi");
            return;
        }
    }
    if (!ensure_task()) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        scanning_ = true;
    }
    const Cmd wake = Cmd::Wake;
    xQueueSend(cmd_queue_, &wake, 0);
}

void Esp32WifiScanner::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        scanning_ = false;
    }
    const Cmd wake = Cmd::Wake;
    xQueueSend(cmd_queue_, &wake, 0);
}

void Esp32WifiScanner::release_radio()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        scanning_         = false;
        release_pending_  = true;
    }
    if (!ensure_task()) {
        tear_down_wifi_stack();
        return;
    }
    const Cmd cmd = Cmd::ReleaseRadio;
    xQueueSend(cmd_queue_, &cmd, 0);
    if (events_ != nullptr) {
        xEventGroupWaitBits(events_, kRadioIdleBit, pdFALSE, pdTRUE, pdMS_TO_TICKS(15000));
    }
}

bool Esp32WifiScanner::is_scanning() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return scanning_;
}

bool Esp32WifiScanner::is_radio_idle() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return !initialised_ && !release_pending_;
}

std::vector<WifiAccessPoint> Esp32WifiScanner::snapshot()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return aps_;
}

void Esp32WifiScanner::scan_task_trampoline(void* arg)
{
    static_cast<Esp32WifiScanner*>(arg)->scan_loop();
}

void Esp32WifiScanner::scan_loop()
{
    std::map<std::string, std::vector<int>> history;

    while (true) {
        bool active          = false;
        bool exit            = false;
        bool release_pending = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            active          = scanning_;
            exit            = shutdown_;
            release_pending = release_pending_;
        }
        if (exit) {
            tear_down_wifi_stack();
            break;
        }

        if (release_pending) {
            tear_down_wifi_stack();
            Cmd cmd{};
            xQueueReceive(cmd_queue_, &cmd, pdMS_TO_TICKS(250));
            continue;
        }

        if (!active) {
            Cmd cmd{};
            if (xQueueReceive(cmd_queue_, &cmd, pdMS_TO_TICKS(250)) == pdTRUE &&
                cmd == Cmd::ReleaseRadio) {
                tear_down_wifi_stack();
            }
            continue;
        }

        if (!ensure_initialised()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        xEventGroupClearBits(events_, kScanDoneBit);
        wifi_scan_config_t scan_cfg = {};
        scan_cfg.show_hidden        = true;
        esp_err_t ret               = esp_wifi_scan_start(&scan_cfg, false);
        if (ret != ESP_OK) {
            spectra5::log::tagError(kTag, "scan_start failed: {}", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        bool cancelled = false;
        while (true) {
            const EventBits_t bits =
                xEventGroupWaitBits(events_, kScanDoneBit, pdTRUE, pdFALSE, pdMS_TO_TICKS(100));
            if (bits & kScanDoneBit) {
                break;
            }
            bool still           = false;
            bool release_now     = false;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                still       = scanning_;
                release_now = release_pending_;
            }
            if (!still || release_now) {
                esp_wifi_scan_stop();
                xEventGroupWaitBits(events_, kScanDoneBit, pdTRUE, pdFALSE, pdMS_TO_TICKS(2000));
                cancelled = true;
                break;
            }
        }
        if (cancelled) {
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!scanning_ || release_pending_) {
                continue;
            }
        }

        uint16_t count = 0;
        esp_wifi_scan_get_ap_num(&count);
        std::vector<wifi_ap_record_t> records(count);
        if (count > 0) {
            esp_wifi_scan_get_ap_records(&count, records.data());
        }

        const Timestamp now = clock_.now_ms();
        std::vector<WifiAccessPoint> next;
        next.reserve(count);
        for (uint16_t i = 0; i < count; ++i) {
            const auto& rec = records[i];
            WifiAccessPoint ap;
            ap.bssid    = format_bssid(rec.bssid);
            ap.ssid     = reinterpret_cast<const char*>(rec.ssid);
            ap.hidden   = ap.ssid.empty();
            ap.rssi     = rec.rssi;
            ap.channel  = rec.primary;
            ap.band     = WifiBand::GHz24;
            ap.security = map_security(rec.authmode);
            ap.last_seen  = now;
            ap.first_seen = now;

            auto& hist = history[ap.bssid];
            hist.push_back(rec.rssi);
            if (hist.size() > kHistoryMax) {
                hist.erase(hist.begin());
            }
            ap.rssi_history = hist;
            next.push_back(std::move(ap));
        }

        spectra5::log::tagInfo(kTag, "scan complete: {} APs", static_cast<int>(count));
        {
            std::lock_guard<std::mutex> lock(mutex_);
            aps_ = std::move(next);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        task_ = nullptr;
    }
    vTaskDelete(nullptr);
}

}  // namespace spectra5::platform

#include "services/diagnostics/radio_captest.h"

#include <atomic>
#include <cstdint>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "core/diagnostics/log.hpp"

namespace spectra5::diagnostics {
namespace {

constexpr const char* kTag = "captest";

std::atomic<uint32_t> g_promisc_frames{0};

void promiscuous_cb(void* buf, wifi_promiscuous_pkt_type_t type)
{
    (void)buf;
    (void)type;
    g_promisc_frames.fetch_add(1, std::memory_order_relaxed);
}

const char* verdict(esp_err_t rc)
{
    if (rc == ESP_OK) {
        return "PASS       ";
    }
    if (rc == ESP_ERR_NOT_SUPPORTED) {
        return "UNSUPPORTED";
    }
    return "FAIL       ";
}

void report(const char* capability, esp_err_t rc)
{
    spectra5::log::tagInfo(kTag, "[{}] {}  (rc=0x{:x} {})", verdict(rc), capability,
                           static_cast<unsigned>(rc), esp_err_to_name(rc));
}

// Minimal, benign broadcast 802.11 probe-request frame used only to exercise the
// raw-injection path. A probe request advertises nothing and disrupts nothing; we
// only care about the return code of esp_wifi_80211_tx, not the frame landing.
uint8_t kProbeRequest[] = {
    0x40, 0x00,                          // Frame Control: mgmt, subtype = probe req
    0x00, 0x00,                          // Duration
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // Addr1 (DA) broadcast
    0x02, 0x53, 0x50, 0x45, 0x43, 0x05,  // Addr2 (SA) locally-administered
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // Addr3 (BSSID) broadcast
    0x00, 0x00,                          // Sequence control
    0x00, 0x00,                          // SSID element (wildcard, len 0)
    0x01, 0x04, 0x82, 0x84, 0x8b, 0x96,  // Supported rates
};

bool bring_up_wifi_sta()
{
    // The transport to the C6 is already up (HAL/BSP initialised it before this
    // runs). We only need the Wi-Fi MAC layer started; no netif/IP is required to
    // probe channel control, promiscuous mode or raw TX.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret                    = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        report("wifi.init", ret);
        return false;
    }
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        report("wifi.set_mode(STA)", ret);
        return false;
    }
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        report("wifi.start", ret);
        return false;
    }
    return true;
}

}  // namespace

void run_radio_captest()
{
    spectra5::log::tagInfo(kTag, "================ Spectra5 radio capability probe ================");
    spectra5::log::tagInfo(kTag, "Target: ESP32-C6 via esp_hosted / esp_wifi_remote (Tab5 P4 host)");

    if (!bring_up_wifi_sta()) {
        spectra5::log::tagError(kTag, "Wi-Fi STA bring-up failed; aborting probe");
        return;
    }
    spectra5::log::tagInfo(kTag, "Wi-Fi STA up on C6 -- probing primitives...");

    // --- Primitives that recon + targeted attacks need -------------------------
    report("wifi.set_channel(6)            [channel hop]", esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE));

    uint8_t orig_mac[6] = {0};
    esp_wifi_get_mac(WIFI_IF_STA, orig_mac);
    uint8_t spoof_mac[6] = {0x02, 0xde, 0xad, 0xbe, 0xef, 0x01};
    esp_err_t mac_rc     = esp_wifi_set_mac(WIFI_IF_STA, spoof_mac);
    report("wifi.set_mac                   [MAC spoof]", mac_rc);
    if (mac_rc == ESP_OK) {
        esp_wifi_set_mac(WIFI_IF_STA, orig_mac);
    }

    report("wifi.set_mode(APSTA)           [Evil Portal / SoftAP]", esp_wifi_set_mode(WIFI_MODE_APSTA));
    esp_wifi_set_mode(WIFI_MODE_STA);

    // --- The two that make-or-break Marauder/Bruce parity ----------------------
    // Promiscuous / monitor RX: sniffing, pcap, EAPOL/PMKID handshake capture,
    // deauth detection, passive wardriving.
    g_promisc_frames.store(0);
    esp_wifi_set_promiscuous_rx_cb(&promiscuous_cb);
    esp_err_t promisc_rc = esp_wifi_set_promiscuous(true);
    report("wifi.set_promiscuous(true)     [monitor/sniff]", promisc_rc);
    if (promisc_rc == ESP_OK) {
        for (int i = 0; i < 3; ++i) {
            esp_wifi_set_channel(static_cast<uint8_t>(1 + i * 5), WIFI_SECOND_CHAN_NONE);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        esp_wifi_set_promiscuous(false);
        spectra5::log::tagInfo(kTag, "[monitor] frames captured in 3s across ch 1/6/11: {}",
                               g_promisc_frames.load());
    }

    // Raw 802.11 injection: deauth flood, beacon spam, probe flood, karma, evil
    // portal raw frames -- the heart of Marauder's attack suite.
    esp_err_t tx_rc = esp_wifi_80211_tx(WIFI_IF_STA, kProbeRequest, sizeof(kProbeRequest), true);
    report("wifi.80211_tx                  [raw injection]", tx_rc);

    spectra5::log::tagInfo(kTag, "Note: BLE advertising/spam is host-side on the P4 (NimBLE) and is");
    spectra5::log::tagInfo(kTag, "      already exercised by the working BLE scanner -- not re-probed here.");
    spectra5::log::tagInfo(kTag, "================ capability probe complete ================");
}

}  // namespace spectra5::diagnostics

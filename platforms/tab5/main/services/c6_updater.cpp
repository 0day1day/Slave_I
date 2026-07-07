#include "services/c6_updater.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <esp_crc.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs.h>
#include <nvs_flash.h>

#include "boot_splash.hpp"
#include "core/diagnostics/log.hpp"

// esp_hosted host-side OTA RPC helpers (granular begin/write/end). Declared here
// to avoid pulling esp_hosted's internal rpc_wrap.h; the symbols live in the
// linked esp_hosted component.
extern "C" {
int rpc_ota_begin(void);
int rpc_ota_write(uint8_t* ota_data, uint32_t ota_data_len);
int rpc_ota_end(void);
}

namespace spectra5::platform {

namespace {
constexpr const char* kTag = "c6-ota";
constexpr std::size_t kChunk = 1400;  // mirrors esp_hosted's OTA chunk size
}  // namespace

esp_err_t flash_c6_from_file(const char* path)
{
    FILE* f = std::fopen(path, "rb");
    if (f == nullptr) {
        spectra5::log::tagError(kTag, "cannot open C6 image: {}", path);
        return ESP_ERR_NOT_FOUND;
    }

    spectra5::log::tagInfo(kTag, "C6 OTA begin from {}", path);
    if (rpc_ota_begin() != 0) {
        spectra5::log::tagError(kTag, "rpc_ota_begin failed");
        std::fclose(f);
        return ESP_FAIL;
    }

    auto* buf = static_cast<uint8_t*>(std::malloc(kChunk));
    if (buf == nullptr) {
        std::fclose(f);
        return ESP_ERR_NO_MEM;
    }

    std::size_t total = 0;
    int err           = 0;
    std::size_t n;
    while ((n = std::fread(buf, 1, kChunk, f)) > 0) {
        err = rpc_ota_write(buf, static_cast<uint32_t>(n));
        if (err != 0) {
            spectra5::log::tagError(kTag, "rpc_ota_write failed at {} bytes", total);
            break;
        }
        total += n;
        if ((total % (64 * 1024)) < kChunk) {
            spectra5::log::tagInfo(kTag, "C6 OTA {} KB...", total / 1024);
        }
    }

    std::free(buf);
    std::fclose(f);

    if (err != 0) {
        return ESP_FAIL;
    }
    if (rpc_ota_end() != 0) {
        spectra5::log::tagError(kTag, "rpc_ota_end failed");
        return ESP_FAIL;
    }

    spectra5::log::tagInfo(kTag, "C6 OTA complete: {} bytes flashed; C6 will reboot", total);
    return ESP_OK;
}

esp_err_t flash_c6_from_buffer(const uint8_t* data, std::size_t len)
{
    if (data == nullptr || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    spectra5::log::tagInfo(kTag, "C6 OTA begin from embedded image ({} bytes)", len);
    if (rpc_ota_begin() != 0) {
        spectra5::log::tagError(kTag, "rpc_ota_begin failed");
        return ESP_FAIL;
    }

    std::size_t off = 0;
    while (off < len) {
        const std::size_t n = (len - off) < kChunk ? (len - off) : kChunk;
        if (rpc_ota_write(const_cast<uint8_t*>(data + off), static_cast<uint32_t>(n)) != 0) {
            spectra5::log::tagError(kTag, "rpc_ota_write failed at {} bytes", off);
            return ESP_FAIL;
        }
        off += n;
        if ((off % (64 * 1024)) < kChunk) {
            spectra5::log::tagInfo(kTag, "C6 OTA {} KB...", off / 1024);
        }
    }

    if (rpc_ota_end() != 0) {
        spectra5::log::tagError(kTag, "rpc_ota_end failed");
        return ESP_FAIL;
    }
    spectra5::log::tagInfo(kTag, "C6 OTA complete: {} bytes flashed; C6 will reboot", len);
    return ESP_OK;
}

// --- First-boot auto-provisioning -------------------------------------------

// The C6 radio firmware, embedded in this P4 app by CMake (target_add_binary_data).
extern "C" const uint8_t c6_bin_start[] asm("_binary_network_adapter_bin_start");
extern "C" const uint8_t c6_bin_end[] asm("_binary_network_adapter_bin_end");

namespace {
constexpr const char* kNvsNs  = "slavei";
constexpr const char* kNvsKey = "c6_crc";  // CRC32 of the C6 image last flashed

std::size_t embedded_len()
{
    return static_cast<std::size_t>(c6_bin_end - c6_bin_start);
}

uint32_t embedded_crc()
{
    return esp_crc32_le(0, c6_bin_start, embedded_len());
}

// nvs_flash_init() is idempotent across the app; make sure it is usable before we
// read/write our marker (a fresh unit has an unformatted NVS partition).
void ensure_nvs()
{
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
}

void record_provisioned(uint32_t crc)
{
    nvs_handle_t h;
    if (nvs_open(kNvsNs, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u32(h, kNvsKey, crc);
        nvs_commit(h);
        nvs_close(h);
    }
}
}  // namespace

bool c6_needs_provision()
{
    if (embedded_len() == 0) {
        return false;  // no image embedded (shouldn't happen on device builds)
    }
    ensure_nvs();
    const uint32_t want = embedded_crc();

    nvs_handle_t h;
    if (nvs_open(kNvsNs, NVS_READONLY, &h) != ESP_OK) {
        return true;  // namespace absent -> never provisioned
    }
    uint32_t have = 0;
    const esp_err_t r = nvs_get_u32(h, kNvsKey, &have);
    nvs_close(h);
    if (r != ESP_OK) {
        return true;  // key absent -> never provisioned
    }
    return have != want;  // differs -> C6 image changed, reprovision
}

void provision_c6_and_reboot()
{
    const std::size_t len = embedded_len();
    const uint32_t crc    = embedded_crc();
    spectra5::log::tagInfo(kTag, "provisioning C6 radio: embedded image {} bytes, crc {:08x}", len,
                           crc);

    // Bring up the minimal hosted stack (mirrors the proven deploy path). These are
    // idempotent-ish; ignore "already initialised" on a warm path.
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wcfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    const esp_err_t e = flash_c6_from_buffer(c6_bin_start, len);
    if (e == ESP_OK) {
        record_provisioned(crc);
        spectra5::log::tagInfo(kTag, "C6 provisioned OK; rebooting P4 for a clean start");
        show_provisioning_done();            // "RADIO ARMED -- rebooting" before the reset
        vTaskDelay(pdMS_TO_TICKS(1500));     // let it be read
        esp_restart();
    }
    spectra5::log::tagError(kTag,
                            "C6 provisioning failed: {} -- continuing to normal boot "
                            "(radio will report unprovisioned)",
                            esp_err_to_name(e));
}

}  // namespace spectra5::platform

/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 * SPDX-FileCopyrightText: 2026 Spectra5 contributors
 *
 * SPDX-License-Identifier: MIT
 */
#include <memory>

#include <app.h>
#include <hal/hal.h>
#include <esp_lvgl_port.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "boot_splash.hpp"
#include <bsp/m5stack_tab5.h>

#include "application/sessions/session_service.hpp"
#include "application/workflows/workflow_engine.hpp"
#include "core/clock.hpp"
#include "core/diagnostics/log.hpp"
#include "core/event_bus/event_bus.hpp"
#include "core/scheduler/task_manager.hpp"
#include "hal/hal_esp32.h"
#include "esp32_ble_scanner.h"
#include "services/esp32_wifi_scanner.h"
#include "services/radio/ble_scanner.hpp"
#include "services/radio/ble_spammer.hpp"
#include "services/radio/radio_coordinator.hpp"
#include "services/storage/sd_logger.hpp"
#include "services/radio/radio_engine.hpp"
#include "services/radio/evil_portal_service.hpp"
#include "services/radio/wifi_scanner.hpp"
#include "services/evil_portal.hpp"
#include "services/hosted_radio_engine.hpp"
#include "services/c6_updater.hpp"
#include "services/tab5_radio_coordinator.hpp"
#include "services/storage/filesystem_browser.hpp"
#include "services/storage/filesystem_session_store.hpp"
#ifdef SPECTRA5_RADIO_CAPTEST
#include "services/diagnostics/radio_captest.h"
#endif
#ifdef SPECTRA5_C6_OTA
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include "services/c6_updater.hpp"
#endif
#include "services/system/system_service.hpp"
#include "services/tab5_system_service.hpp"

extern "C" void app_main(void)
{
    app::InitCallback_t callback;

    callback.onHalInjection = []() {
        hal::Inject(std::make_unique<HalEsp32>());
        spectra5::services::inject_system_service(std::make_unique<Tab5SystemService>());

        // The microSD is mounted persistently by the HAL at /sd. Persist
        // sessions there when present; otherwise leave the service uninjected so
        // the UI reports it honestly instead of pretending to store data.
        static spectra5::SystemClock clock;
        static spectra5::core::EventBus event_bus;
        if (GetHAL()->isSdCardMounted()) {
            static spectra5::services::FilesystemSessionStore session_store("/sd/spectra5/sessions");
            spectra5::application::inject_session_service(
                std::make_unique<spectra5::application::SessionService>(session_store, clock,
                                                                        &event_bus));
            static spectra5::services::FilesystemBrowser browser("/sd");
            spectra5::services::inject_storage_browser(&browser);

            static spectra5::core::TaskManager task_manager;
            spectra5::application::inject_workflow_engine(
                std::make_unique<spectra5::application::WorkflowEngine>(
                    *spectra5::application::session_service(), task_manager));
            spectra5::log::tagInfo("app", "session store + file browser + workflows ready on /sd");
        } else {
            spectra5::log::tagWarn("app", "microSD not mounted; sessions/files disabled");
        }

        // Real Wi-Fi scanner backed by the ESP32-C6 (esp_wifi_remote). Radio is
        // initialised lazily on the first scan; independent of the microSD.
        static spectra5::platform::Esp32WifiScanner wifi(clock);
        spectra5::services::inject_wifi_scanner(&wifi);

        static spectra5::platform::Esp32BleScanner ble(clock);
        spectra5::services::inject_ble_scanner(&ble);
        spectra5::services::inject_ble_spammer(&ble);

        static spectra5::platform::Tab5RadioCoordinator radio;
        radio.bind(&wifi, &ble);
        spectra5::services::inject_radio_coordinator(&radio);

        // Real offensive engine: P4 -> C6 commands over the esp_hosted side-channel.
        static spectra5::platform::HostedRadioEngine radio_engine;
        spectra5::services::inject_radio_engine(&radio_engine);

        spectra5::services::inject_evil_portal(&spectra5::platform::EvilPortal::instance());

        spectra5::services::SdLogger::instance().ensure_dirs();  // structured SD log tree
    };

    // The Boba-Fett boot splash is now raised from the AppShell constructor
    // (inside app::Init) so it covers the UI as it builds, instead of flashing
    // the dashboard first. See spectra5_show_boot_splash() in boot_splash.cpp.
    app::Init(callback);

#ifdef SPECTRA5_C6_OTA
    // Deploy build: bring up the hosted RPC (a minimal Wi-Fi init does this) and
    // push our C6 firmware (embedded in this build, no microSD) to the C6, then
    // halt. Reflash the normal P4 firmware afterwards.
    {
        esp_err_t nvs = nvs_flash_init();
        if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            nvs_flash_erase();
            nvs_flash_init();
        }
        esp_netif_init();
        esp_event_loop_create_default();
        wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&wcfg);
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
        extern const uint8_t c6_bin_start[] asm("_binary_network_adapter_bin_start");
        extern const uint8_t c6_bin_end[] asm("_binary_network_adapter_bin_end");
        const std::size_t c6_len = static_cast<std::size_t>(c6_bin_end - c6_bin_start);
        const esp_err_t e = spectra5::platform::flash_c6_from_buffer(c6_bin_start, c6_len);
        spectra5::log::tagInfo("app", "C6 OTA result: {} ({} bytes)", esp_err_to_name(e), c6_len);
    }
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif

#ifdef SPECTRA5_RADIO_CAPTEST
    // Diagnostic build: probe the C6 radio capabilities once, then halt so the
    // result stays on screen/serial and the normal app never starts. Reflash the
    // standard firmware to resume normal operation.
    spectra5::diagnostics::run_radio_captest();
    spectra5::log::tagInfo("app", "captest build halted; reflash normal firmware to resume");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif

    // First-boot (or post-update) auto-provisioning of the C6 radio from the image
    // embedded in this firmware. Skipped on normal boots (NVS remembers the flashed
    // image CRC). On success the P4 reboots into a clean start; on failure it logs
    // and falls through so the app still runs (the radio just reports unprovisioned).
    if (spectra5::platform::c6_needs_provision()) {
        spectra5::log::tagInfo("app", "C6 radio not provisioned -- flashing embedded image");
        spectra5::platform::show_provisioning_overlay();  // spinner, so it doesn't look like a crash
        spectra5::platform::provision_c6_and_reboot();
    }

    while (!app::IsDone()) {
        app::Update();
        vTaskDelay(1);
    }
    app::Destroy();
}

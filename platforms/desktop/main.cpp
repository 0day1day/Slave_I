/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 * SPDX-FileCopyrightText: 2026 Slave I contributors
 *
 * SPDX-License-Identifier: MIT
 */
#include <cstdlib>
#include <memory>

#include <app.h>
#include <hal/hal.h>

#include "application/sessions/session_service.hpp"
#include "application/workflows/workflow_engine.hpp"
#include "core/clock.hpp"
#include "core/event_bus/event_bus.hpp"
#include "core/scheduler/task_manager.hpp"
#include "domain/radio/wifi.hpp"
#include "hal/hal_desktop.h"
#include "mocks/mock_system_service.hpp"
#include "services/radio/evil_portal_service.hpp"
#include "services/radio/mock_radio_engine.hpp"
#include "services/radio/radio_engine.hpp"
#include "services/radio/wifi_scanner.hpp"
#include "services/storage/filesystem_browser.hpp"
#include "services/storage/filesystem_session_store.hpp"
#include "services/system/system_service.hpp"

// Desktop stubs for the platform hooks the UI calls (device provides strong
// overrides: boot_splash.cpp / about_overlay.cpp / power_control.cpp / hal_usb.cpp).
// These MUST be plain (non-weak) definitions and the app side MUST only declare
// them -- a weak def in the app layer makes the ESP-IDF linker drop the strong
// device objects, so the splash / About overlay silently never run.
extern "C" void spectra5_show_boot_splash() {}
extern "C" void spectra5_show_about() {}
extern "C" void spectra5_sleep() {}
extern "C" void spectra5_power_off() {}
extern "C" struct _lv_group_t* spectra5_nav_group() { return nullptr; }
extern "C" bool spectra5_keyboard_connected() { return false; }
extern "C" void spectra5_screenshot() {}

namespace {

// Fake Wi-Fi scanner so the emulator shows a populated AP list (and the offensive
// target view / attack modals) without radio hardware.
class MockWifiScanner final : public spectra5::services::IWifiScanner {
public:
    void start() override { scanning_ = true; }
    void stop() override { scanning_ = false; }
    bool is_scanning() const override { return scanning_; }
    std::vector<spectra5::domain::WifiAccessPoint> snapshot() override { return aps_; }

private:
    using AP = spectra5::domain::WifiAccessPoint;
    bool scanning_ = true;
    std::vector<AP> aps_ = [] {
        using B = spectra5::domain::WifiBand;
        using S = spectra5::domain::WifiSecurity;
        std::vector<AP> v;
        v.push_back({.bssid = "00:11:22:33:44:55", .ssid = "HomeNet", .channel = 6, .band = B::GHz24,
                     .rssi = -42, .security = S::Wpa2, .rssi_history = {-44, -42, -41}});
        v.push_back({.bssid = "a4:2b:b0:9c:1d:e0", .ssid = "Mi Casa 2.4", .channel = 1,
                     .band = B::GHz24, .rssi = -58, .security = S::Wpa3, .rssi_history = {-60, -58}});
        v.push_back({.bssid = "de:ad:be:ef:00:01", .ssid = "Cafe Guest", .channel = 11,
                     .band = B::GHz24, .rssi = -71, .security = S::Open, .rssi_history = {-73, -71}});
        v.push_back({.bssid = "12:34:56:78:9a:bc", .ssid = "", .channel = 3, .band = B::GHz24,
                     .rssi = -80, .security = S::Wpa2, .hidden = true, .rssi_history = {-82, -80}});
        return v;
    }();
};

// Trivial Evil Portal so the header button + modal appear in the emulator.
class MockEvilPortal final : public spectra5::services::IEvilPortal {
public:
    bool start(const std::string&) override { active_ = true; return true; }
    void stop() override { active_ = false; }
    bool active() const override { return active_; }
    int captured() const override { return 0; }

private:
    bool active_ = false;
};

}  // namespace

int main()
{
    app::InitCallback_t callback;

    callback.onHalInjection = []() {
        hal::Inject(std::make_unique<HalDesktop>());
        spectra5::services::inject_system_service(std::make_unique<MockSystemService>());

        // Real session persistence on the host filesystem.
        static spectra5::SystemClock clock;
        static spectra5::core::EventBus event_bus;
        static spectra5::services::FilesystemSessionStore session_store("spectra5_data/sessions");
        spectra5::application::inject_session_service(
            std::make_unique<spectra5::application::SessionService>(session_store, clock,
                                                                    &event_bus));

        // File browser rooted at the local data dir (mirrors the Tab5 /sd root).
        static spectra5::services::FilesystemBrowser browser("spectra5_data");
        spectra5::services::inject_storage_browser(&browser);

        // Workflow engine drives real session use cases on a background task.
        static spectra5::core::TaskManager task_manager;
        spectra5::application::inject_workflow_engine(
            std::make_unique<spectra5::application::WorkflowEngine>(
                *spectra5::application::session_service(), task_manager));

        // Mock radio stack so the emulator can show the offensive UI (scan list,
        // AP target view, Deauth/Scan-clients modals, Evil Portal) without hardware.
        static MockWifiScanner wifi;
        spectra5::services::inject_wifi_scanner(&wifi);
        static spectra5::services::MockRadioEngine radio_engine;
        spectra5::services::inject_radio_engine(&radio_engine);
        static MockEvilPortal portal;
        spectra5::services::inject_evil_portal(&portal);
    };

    app::Init(callback);
    while (!app::IsDone()) {
        app::Update();
    }
    app::Destroy();

    return 0;
}

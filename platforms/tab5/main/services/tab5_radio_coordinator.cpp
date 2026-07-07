#include "tab5_radio_coordinator.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "core/diagnostics/log.hpp"

namespace spectra5::platform {

namespace {
constexpr const char* kTag = "radio-c6";
}

void Tab5RadioCoordinator::bind(services::IWifiScanner* wifi, services::IBleScanner* ble)
{
    wifi_ = wifi;
    ble_  = ble;
}

bool Tab5RadioCoordinator::wait_idle(services::IWifiScanner* wifi, services::IBleScanner* ble,
                                     int timeout_ms)
{
    const int steps = timeout_ms / 50;
    for (int i = 0; i < steps; ++i) {
        const bool wifi_ok = (wifi == nullptr) || wifi->is_radio_idle();
        const bool ble_ok  = (ble == nullptr) || ble->is_radio_idle();
        if (wifi_ok && ble_ok) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return false;
}

bool Tab5RadioCoordinator::acquire_for_wifi()
{
    prepare_wifi_route();
    return true;
}

bool Tab5RadioCoordinator::acquire_for_ble()
{
    prepare_ble_route();
    return true;
}

void Tab5RadioCoordinator::prepare_wifi_route()
{
    if (ble_ == nullptr) {
        return;
    }
    spectra5::log::tagInfo(kTag, "route Wi-Fi: stop BLE and release C6 radio");
    ble_->stop();
    ble_->release_radio();
    if (!wait_idle(nullptr, ble_, 15000)) {
        spectra5::log::tagError(kTag, "timeout waiting for BLE radio idle (Wi-Fi route)");
    }
    vTaskDelay(pdMS_TO_TICKS(300));
}

void Tab5RadioCoordinator::prepare_ble_route()
{
    if (wifi_ == nullptr) {
        return;
    }
    spectra5::log::tagInfo(kTag, "route BLE: stop Wi-Fi and release C6 radio");
    wifi_->stop();
    wifi_->release_radio();
    if (!wait_idle(wifi_, nullptr, 15000)) {
        spectra5::log::tagError(kTag, "timeout waiting for Wi-Fi radio idle (BLE route)");
    }
    vTaskDelay(pdMS_TO_TICKS(300));
}

void Tab5RadioCoordinator::release_all()
{
    if (wifi_ != nullptr) {
        wifi_->stop();
        wifi_->release_radio();
    }
    if (ble_ != nullptr) {
        ble_->stop();
        ble_->release_radio();
    }
    if (!wait_idle(wifi_, ble_, 15000)) {
        spectra5::log::tagWarn(kTag, "timeout waiting for all radios idle");
    }
    vTaskDelay(pdMS_TO_TICKS(200));
}

}  // namespace spectra5::platform

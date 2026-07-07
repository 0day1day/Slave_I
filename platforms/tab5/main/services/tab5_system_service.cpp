#include "tab5_system_service.hpp"

#include <algorithm>

#include <esp_heap_caps.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "hal/hal.h"
#include "services/radio/ble_scanner.hpp"
#include "services/radio/wifi_scanner.hpp"

using namespace spectra5::services;

namespace {

// Estimación de carga de batería 2S a partir del voltaje de bus medido por
// el INA226. Mapeo lineal documentado (no es una lectura de fuel-gauge).
float estimate_battery_percent(float bus_voltage)
{
    constexpr float kEmpty = 6.6f;
    constexpr float kFull  = 8.4f;
    if (bus_voltage <= kEmpty) {
        return 0.0f;
    }
    if (bus_voltage >= kFull) {
        return 100.0f;
    }
    return (bus_voltage - kEmpty) / (kFull - kEmpty) * 100.0f;
}

}  // namespace

SystemMetrics Tab5SystemService::metrics()
{
    SystemMetrics m;

    auto* hal = GetHAL();
    hal->updatePowerMonitorData();

    m.bus_voltage     = hal->powerMonitorData.busVoltage;
    m.bus_current     = hal->powerMonitorData.shuntCurrent;
    m.charging        = hal->getChargeEnable();
    m.battery_percent = estimate_battery_percent(m.bus_voltage);

    m.cpu_temp_c       = hal->getCpuTemp();
    m.free_heap_bytes  = esp_get_free_heap_size();
    m.total_heap_bytes = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);

    m.sd_mounted = hal->isSdCardMounted();

    // Actividad de radio + dispositivos observados: datos reales de los scanners.
    int devices   = 0;
    bool scanning = false;
    if (auto* w = spectra5::services::wifi_scanner()) {
        devices += static_cast<int>(w->snapshot().size());
        scanning = scanning || w->is_scanning();
    }
    if (auto* b = spectra5::services::ble_scanner()) {
        devices += static_cast<int>(b->snapshot().size());
        scanning = scanning || b->is_scanning();
    }
    m.observed_devices = devices;
    m.radio_activity   = scanning ? RadioActivity::Scanning : RadioActivity::Idle;
    m.running_tasks    = static_cast<int>(uxTaskGetNumberOfTasks());

    // Liveness del C6: sólo lo damos por "Connected" cuando hay evidencia real de
    // que responde (un scan en curso o resultados recibidos vía esp_wifi_remote).
    // Si aún no se ha usado la radio, es honesto reportar "Unknown".
    m.c6_status = (scanning || devices > 0) ? CoprocessorStatus::Connected
                                            : CoprocessorStatus::Unknown;

    return m;
}

CapabilitySet Tab5SystemService::capabilities()
{
    // Solo capacidades realmente integradas en hardware en esta fase.
    // WifiScan: escaneo real vía el coprocesador C6 (esp_wifi_remote).
    return CapabilitySet{
        Capability::WifiScan,
        Capability::BleScan,
        Capability::StorageSd,
        Capability::UsbHost,
        Capability::Rs485,
        Capability::Camera,
        Capability::Audio,
    };
}

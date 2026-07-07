#pragma once

#include <cstdint>
#include <memory>

#include "services/capabilities/capability.hpp"

// Servicio de estado de sistema para el dashboard (sección 13 del PRD).
// Interfaz pura: sin LVGL/ESP-IDF. Las plataformas inyectan su implementación.
namespace spectra5::services {

enum class CoprocessorStatus : std::uint8_t {
    Unknown,
    Connected,
    Disconnected,
    Error,
};

enum class RadioActivity : std::uint8_t {
    Idle,
    Scanning,
};

struct SystemMetrics {
    // Energía
    float battery_percent = 0.0f;
    float bus_voltage     = 0.0f;
    float bus_current     = 0.0f;
    bool charging         = false;

    // Térmico / memoria
    int cpu_temp_c            = 0;
    std::uint32_t free_heap_bytes  = 0;
    std::uint32_t total_heap_bytes = 0;

    // Almacenamiento
    bool sd_mounted = false;

    // Coprocesador y radio
    CoprocessorStatus c6_status = CoprocessorStatus::Unknown;
    RadioActivity radio_activity = RadioActivity::Idle;

    // Investigación
    bool session_active     = false;
    int observed_devices    = 0;
    int recent_alerts       = 0;
    int running_tasks       = 0;
    int external_peripherals = 0;
};

class ISystemService {
public:
    virtual ~ISystemService() = default;

    // Lee el estado actual. Debe ser barato y no bloqueante.
    virtual SystemMetrics metrics() = 0;

    // Capacidades publicadas por la plataforma.
    virtual CapabilitySet capabilities() = 0;
};

// Singleton inyectable (patrón equivalente al HAL).
ISystemService* system_service();
void inject_system_service(std::unique_ptr<ISystemService> service);
void destroy_system_service();
bool has_system_service();

}  // namespace spectra5::services

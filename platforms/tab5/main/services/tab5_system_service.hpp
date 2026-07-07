#pragma once

#include "services/system/system_service.hpp"

// Servicio de sistema para Tab5: lee datos reales del HAL y del runtime
// ESP-IDF. No reporta capacidades que aún no estén integradas (honestidad
// de Fase 1: Wi-Fi/BLE reales llegan con la integración del C6).
class Tab5SystemService : public spectra5::services::ISystemService {
public:
    spectra5::services::SystemMetrics metrics() override;
    spectra5::services::CapabilitySet capabilities() override;
};

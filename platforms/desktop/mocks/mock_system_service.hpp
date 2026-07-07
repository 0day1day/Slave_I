#pragma once

#include "services/system/system_service.hpp"

// Implementación mock determinista del servicio de sistema para desktop.
// Los valores evolucionan de forma reproducible según un contador de pasos,
// lo que permite capturas deterministas (sección 10.4 del PRD).
class MockSystemService : public spectra5::services::ISystemService {
public:
    MockSystemService();

    spectra5::services::SystemMetrics metrics() override;
    spectra5::services::CapabilitySet capabilities() override;

    // Permite a los escenarios desktop fijar un estado base.
    void set_base(const spectra5::services::SystemMetrics& base);
    void set_capabilities(const spectra5::services::CapabilitySet& caps);

private:
    spectra5::services::SystemMetrics base_;
    spectra5::services::CapabilitySet caps_;
    std::uint32_t step_ = 0;
};

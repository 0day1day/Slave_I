#include "mock_system_service.hpp"

#include <cmath>

using namespace spectra5::services;

MockSystemService::MockSystemService()
{
    base_.battery_percent     = 76.0f;
    base_.bus_voltage         = 7.8f;
    base_.bus_current         = 0.42f;
    base_.charging            = false;
    base_.cpu_temp_c          = 44;
    base_.free_heap_bytes     = 22u * 1024u * 1024u;
    base_.total_heap_bytes    = 32u * 1024u * 1024u;
    base_.sd_mounted          = true;
    base_.c6_status           = CoprocessorStatus::Connected;
    base_.radio_activity      = RadioActivity::Idle;
    base_.session_active      = false;
    base_.observed_devices    = 0;
    base_.recent_alerts       = 0;
    base_.running_tasks       = 0;
    base_.external_peripherals = 0;

    caps_ = CapabilitySet{
        Capability::WifiScan,    Capability::WifiMonitor, Capability::BleScan,
        Capability::BleGattClient, Capability::Ieee802154Scan, Capability::StorageSd,
        Capability::UsbHost,     Capability::Rs485,       Capability::Audio,
    };
}

SystemMetrics MockSystemService::metrics()
{
    SystemMetrics m = base_;

    // Variación determinista y suave en función del paso.
    const double phase = static_cast<double>(step_) * 0.15;
    m.cpu_temp_c       = base_.cpu_temp_c + static_cast<int>(std::lround(2.0 * std::sin(phase)));
    m.bus_current      = base_.bus_current + 0.05f * static_cast<float>(std::sin(phase * 1.7));

    ++step_;
    return m;
}

CapabilitySet MockSystemService::capabilities()
{
    return caps_;
}

void MockSystemService::set_base(const SystemMetrics& base)
{
    base_ = base;
}

void MockSystemService::set_capabilities(const CapabilitySet& caps)
{
    caps_ = caps;
}

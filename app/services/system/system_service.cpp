#include "services/system/system_service.hpp"

#include "core/diagnostics/log.hpp"

namespace spectra5::services {

namespace {
constexpr const char* kTag = "system-service";

class NullSystemService : public ISystemService {
public:
    SystemMetrics metrics() override { return {}; }
    CapabilitySet capabilities() override { return {}; }
};

std::unique_ptr<ISystemService> g_service;
}  // namespace

ISystemService* system_service()
{
    if (!g_service) {
        spectra5::log::tagWarn(kTag, "getting null system service, auto inject null impl");
        g_service = std::make_unique<NullSystemService>();
    }
    return g_service.get();
}

void inject_system_service(std::unique_ptr<ISystemService> service)
{
    if (!service) {
        spectra5::log::tagError(kTag, "pass null system service");
        return;
    }
    g_service = std::move(service);
    spectra5::log::tagInfo(kTag, "system service injected");
}

void destroy_system_service()
{
    g_service.reset();
}

bool has_system_service()
{
    return static_cast<bool>(g_service);
}

}  // namespace spectra5::services

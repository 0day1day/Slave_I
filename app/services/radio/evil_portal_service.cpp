#include "services/radio/evil_portal_service.hpp"

namespace spectra5::services {

namespace {
IEvilPortal* g_portal = nullptr;
}

IEvilPortal* evil_portal()
{
    return g_portal;
}

void inject_evil_portal(IEvilPortal* portal)
{
    g_portal = portal;
}

bool has_evil_portal()
{
    return g_portal != nullptr;
}

}  // namespace spectra5::services

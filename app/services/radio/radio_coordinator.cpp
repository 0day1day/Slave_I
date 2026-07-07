#include "services/radio/radio_coordinator.hpp"

namespace spectra5::services {

namespace {
IRadioCoordinator* g_coordinator = nullptr;
}

IRadioCoordinator* radio_coordinator()
{
    return g_coordinator;
}

void inject_radio_coordinator(IRadioCoordinator* coordinator)
{
    g_coordinator = coordinator;
}

}  // namespace spectra5::services

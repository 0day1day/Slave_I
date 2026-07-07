#include "services/radio/ble_spammer.hpp"

namespace spectra5::services {

namespace {
IBleSpammer* g_spammer = nullptr;
}

IBleSpammer* ble_spammer()
{
    return g_spammer;
}

void inject_ble_spammer(IBleSpammer* spammer)
{
    g_spammer = spammer;
}

bool has_ble_spammer()
{
    return g_spammer != nullptr;
}

}  // namespace spectra5::services

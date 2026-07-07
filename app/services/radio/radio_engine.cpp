#include "services/radio/radio_engine.hpp"

namespace spectra5::services {

namespace {
IRadioEngine* g_engine = nullptr;
}

IRadioEngine* radio_engine()
{
    return g_engine;
}

void inject_radio_engine(IRadioEngine* engine)
{
    g_engine = engine;
}

bool has_radio_engine()
{
    return g_engine != nullptr;
}

}  // namespace spectra5::services

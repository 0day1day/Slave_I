#include "bootstrap.hpp"

#include <memory>

#include "app_controller.hpp"
#include "core/diagnostics/log.hpp"
#include "hal/hal.h"

namespace spectra5::application {

namespace {
constexpr const char* kTag = "bootstrap";

std::unique_ptr<AppController> g_controller;
}

void bootstrap(const BootstrapCallbacks& callbacks)
{
    spectra5::log::tagInfo(kTag, "starting");

    if (callbacks.on_hal_injection) {
        spectra5::log::tagInfo(kTag, "hal injection");
        callbacks.on_hal_injection();
    }

    g_controller = std::make_unique<AppController>();
    g_controller->start();
}

void tick()
{
    if (g_controller) {
        g_controller->update();
    }
}

bool is_done()
{
    return false;
}

void shutdown()
{
    if (g_controller) {
        g_controller->stop();
        g_controller.reset();
    }
    hal::Destroy();
    spectra5::log::tagInfo(kTag, "shutdown complete");
}

}  // namespace spectra5::application

#pragma once

#include <functional>

namespace spectra5::application {

struct BootstrapCallbacks {
    std::function<void()> on_hal_injection;
};

void bootstrap(const BootstrapCallbacks& callbacks);
void tick();
bool is_done();
void shutdown();

}  // namespace spectra5::application

#pragma once

#include <cstdint>
#include <memory>

#include "ui/app_shell.hpp"

namespace spectra5::application {

class AppController {
public:
    void start();
    void update();
    void stop();

private:
    void refresh();

    bool started_                 = false;
    std::uint32_t last_refresh_ms_ = 0;
    std::unique_ptr<ui::AppShell> shell_;
};

}  // namespace spectra5::application

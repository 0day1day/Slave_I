/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 * SPDX-FileCopyrightText: 2026 Slave I contributors
 *
 * SPDX-License-Identifier: MIT
 */
#include "app.h"

#include <thread>

#include "application/bootstrap.hpp"
#include "core/diagnostics/log.hpp"
#include "hal/hal.h"
#include <lvgl.h>

namespace {
constexpr const char* kTag = "app";
}

void app::Init(InitCallback_t callback)
{
    spectra5::application::BootstrapCallbacks bootstrap_callbacks;
    bootstrap_callbacks.on_hal_injection = callback.onHalInjection;
    spectra5::application::bootstrap(bootstrap_callbacks);
}

void app::Update()
{
    spectra5::application::tick();

#if defined(__APPLE__) && defined(__MACH__)
    GetHAL()->lvglLock();
    const auto time_till_next = lv_timer_handler();
    GetHAL()->lvglUnlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(time_till_next));
#endif
}

bool app::IsDone()
{
    return spectra5::application::is_done();
}

void app::Destroy()
{
    spectra5::application::shutdown();
}

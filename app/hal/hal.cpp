/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 * SPDX-FileCopyrightText: 2026 Slave I contributors
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal.h"

#include <memory>

#include "core/diagnostics/log.hpp"

namespace {
constexpr const char* kTag = "hal";
}

static std::unique_ptr<hal::HalBase> g_hal_instance;

hal::HalBase* hal::Get()
{
    if (!g_hal_instance) {
        spectra5::log::tagWarn(kTag, "getting null hal, auto inject base");
        g_hal_instance = std::make_unique<HalBase>();
    }
    return g_hal_instance.get();
}

void hal::Inject(std::unique_ptr<HalBase> hal_impl)
{
    if (!hal_impl) {
        spectra5::log::tagError(kTag, "pass null hal");
        return;
    }

    Destroy();
    g_hal_instance = std::move(hal_impl);

    spectra5::log::tagInfo(kTag, "injecting hal type: {}", g_hal_instance->type());
    spectra5::log::tagInfo(kTag, "invoke init");
    g_hal_instance->init();
    spectra5::log::tagInfo(kTag, "hal injected");
}

void hal::Destroy()
{
    g_hal_instance.reset();
}

bool hal::Check()
{
    return static_cast<bool>(g_hal_instance);
}

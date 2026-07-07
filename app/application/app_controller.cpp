#include "app_controller.hpp"

#include <cstdio>
#include <ctime>

#include <lvgl.h>

#include "core/diagnostics/log.hpp"
#include "core/version.hpp"
#include "hal/hal.h"
#include "services/system/system_service.hpp"
#include "ui/design_system/lv_color.hpp"

namespace spectra5::application {

namespace {
constexpr const char* kTag = "app";
constexpr std::uint32_t kRefreshIntervalMs = 500;
}  // namespace

void AppController::start()
{
    if (started_) {
        return;
    }

    auto caps = services::system_service()->capabilities();

    GetHAL()->lvglLock();
    lv_obj_t* screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, ui::lv_semantic(ui::tokens::SemanticColor::Background), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    shell_ = std::make_unique<ui::AppShell>(screen, caps);
    GetHAL()->lvglUnlock();

    started_ = true;
    spectra5::log::tagInfo(kTag, "app shell ready ({}x{})", spectra5::kDisplayWidth, spectra5::kDisplayHeight);

    refresh();
}

void AppController::update()
{
    if (!started_) {
        start();
        return;
    }

    const std::uint32_t now = GetHAL()->millis();
    if (now - last_refresh_ms_ < kRefreshIntervalMs) {
        return;
    }
    last_refresh_ms_ = now;
    refresh();
}

void AppController::refresh()
{
    if (!shell_) {
        return;
    }

    auto metrics = services::system_service()->metrics();

    char clock_buf[40] = "--/--/---- --:--";
    std::time_t t = std::time(nullptr);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &t);
    std::tm* lt = &tm_buf;
#else
    std::tm* lt = localtime_r(&t, &tm_buf);
#endif
    if (lt != nullptr) {
        std::snprintf(clock_buf, sizeof(clock_buf), "%02d/%02d/%04d  %02d:%02d", lt->tm_mday,
                      lt->tm_mon + 1, lt->tm_year + 1900, lt->tm_hour, lt->tm_min);
    }

    GetHAL()->lvglLock();
    shell_->set_clock(clock_buf);
    shell_->refresh(metrics);
    GetHAL()->lvglUnlock();
}

void AppController::stop()
{
    shell_.reset();
    started_ = false;
}

}  // namespace spectra5::application

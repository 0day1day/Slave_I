#include "ui/components/status_bar.hpp"

#include <cstdio>

#include "core/version.hpp"
#include "ui/design_system/lv_color.hpp"

// True when the M5 I2C keyboard is connected (HAL on device, desktop stub).
extern "C" bool spectra5_keyboard_connected();

// Capture the screen to microSD (device: screenshot.cpp; desktop: stub). Hidden
// trigger for clean demo stills: long-press the status-bar wordmark.
extern "C" void spectra5_screenshot();

namespace spectra5::ui {

using tokens::SemanticColor;
using services::CoprocessorStatus;

namespace {

lv_obj_t* make_label(lv_obj_t* parent, SemanticColor color)
{
    lv_obj_t* lbl = lv_label_create(parent);
    lv_obj_set_style_text_color(lbl, lv_semantic(color), 0);
    lv_obj_set_style_text_font(lbl, &ibm_plex_mono_16, 0);
    return lbl;
}

SemanticColor c6_color(CoprocessorStatus status)
{
    switch (status) {
        case CoprocessorStatus::Connected:    return SemanticColor::Success;
        case CoprocessorStatus::Disconnected: return SemanticColor::Danger;
        case CoprocessorStatus::Error:        return SemanticColor::Danger;
        case CoprocessorStatus::Unknown:
        default:                              return SemanticColor::TextSecondary;
    }
}

const char* c6_text(CoprocessorStatus status)
{
    switch (status) {
        case CoprocessorStatus::Connected:    return "C6 OK";
        case CoprocessorStatus::Disconnected: return "C6 OFF";
        case CoprocessorStatus::Error:        return "C6 ERR";
        case CoprocessorStatus::Unknown:
        default:                              return "C6 --";
    }
}

}  // namespace

StatusBar::StatusBar(lv_obj_t* parent)
{
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, lv_pct(100), tokens::StatusBarHeight);
    lv_obj_set_style_bg_color(root_, lv_semantic(SemanticColor::Surface), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_border_side(root_, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(root_, 1, 0);
    lv_obj_set_style_border_color(root_, lv_semantic(SemanticColor::Border), 0);
    lv_obj_set_style_pad_hor(root_, tokens::SpaceMd, 0);
    lv_obj_set_style_pad_ver(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    // Hidden screenshot trigger: long-press ANYWHERE on the top bar -> capture to SD.
    // Big, forgiving target; no visible UI added to the frame.
    lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        root_, [](lv_event_t*) { spectra5_screenshot(); }, LV_EVENT_LONG_PRESSED, nullptr);

    lv_obj_t* wordmark = lv_label_create(root_);
    lv_label_set_text(wordmark, spectra5::kProjectName);
    lv_obj_set_style_text_color(wordmark, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(wordmark, &ibm_plex_mono_20, 0);
    lv_obj_align(wordmark, LV_ALIGN_LEFT_MID, 0, 0);

    clock_ = make_label(root_, SemanticColor::TextPrimary);
    lv_label_set_text(clock_, "--:--");
    lv_obj_align(clock_, LV_ALIGN_RIGHT_MID, 0, 0);

    battery_ = make_label(root_, SemanticColor::TextSecondary);
    lv_obj_align(battery_, LV_ALIGN_RIGHT_MID, -200, 0);
    lv_label_set_text(battery_, "-- %");

    sd_ = make_label(root_, SemanticColor::TextSecondary);
    lv_obj_align(sd_, LV_ALIGN_RIGHT_MID, -300, 0);
    lv_label_set_text(sd_, "SD --");

    c6_ = make_label(root_, SemanticColor::TextSecondary);
    lv_obj_align(c6_, LV_ALIGN_RIGHT_MID, -400, 0);
    lv_label_set_text(c6_, "C6 --");

    // Keyboard indicator: shown only while the M5 I2C keyboard is connected.
    kbd_ = make_label(root_, SemanticColor::Accent);
    lv_label_set_text(kbd_, LV_SYMBOL_KEYBOARD);
    lv_obj_align(kbd_, LV_ALIGN_RIGHT_MID, -490, 0);
    lv_obj_add_flag(kbd_, LV_OBJ_FLAG_HIDDEN);
}

void StatusBar::update(const services::SystemMetrics& metrics)
{
    lv_obj_set_style_text_color(c6_, lv_semantic(c6_color(metrics.c6_status)), 0);
    lv_label_set_text(c6_, c6_text(metrics.c6_status));

    lv_obj_set_style_text_color(
        sd_, lv_semantic(metrics.sd_mounted ? SemanticColor::Success : SemanticColor::TextSecondary), 0);
    lv_label_set_text(sd_, metrics.sd_mounted ? "SD OK" : "SD --");

    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d %%", static_cast<int>(metrics.battery_percent + 0.5f));
    SemanticColor batt_color = SemanticColor::TextSecondary;
    if (metrics.battery_percent < 15.0f) {
        batt_color = SemanticColor::Danger;
    } else if (metrics.charging) {
        batt_color = SemanticColor::Success;
    }
    lv_obj_set_style_text_color(battery_, lv_semantic(batt_color), 0);
    lv_label_set_text(battery_, buf);

    if (kbd_ != nullptr) {
        if (spectra5_keyboard_connected()) {
            lv_obj_clear_flag(kbd_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(kbd_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void StatusBar::set_clock(const char* hhmm)
{
    lv_label_set_text(clock_, hhmm);
}

}  // namespace spectra5::ui

#pragma once

#include <functional>

#include <lvgl.h>

#include "ui/design_system/theme.hpp"

namespace spectra5::ui {

// Real Settings screen: theme selection (applied live), display brightness
// (applied through the HAL) and device information. No placeholders.
class SettingsScreen {
public:
    using ThemeChanged = std::function<void(ThemeId)>;

    SettingsScreen(lv_obj_t* parent, ThemeChanged on_theme_changed);

private:
    static void on_theme_click(lv_event_t* event);
    static void on_brightness_changed(lv_event_t* event);
    static void on_volume_changed(lv_event_t* event);
    static void on_set_time_click(lv_event_t* event);
    static void on_time_save(lv_event_t* event);
    static void on_time_cancel(lv_event_t* event);

    lv_obj_t* root_            = nullptr;
    lv_obj_t* brightness_lbl_  = nullptr;
    lv_obj_t* volume_lbl_      = nullptr;
    lv_obj_t* clock_lbl_       = nullptr;
    lv_obj_t* dt_modal_        = nullptr;
    lv_obj_t* date_ta_         = nullptr;
    lv_obj_t* time_ta_         = nullptr;
    ThemeChanged on_theme_changed_;
};

}  // namespace spectra5::ui

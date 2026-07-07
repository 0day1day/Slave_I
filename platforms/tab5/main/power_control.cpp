#include <lvgl.h>

#include "bsp/display.h"
#include "bsp/m5stack_tab5.h"

// Power controls exposed to the cross-platform UI via weak hooks (strong here,
// weak no-ops on desktop). Wired to the Settings screen's Sleep / Power off.

namespace {
void wake_on_touch(lv_event_t* e)
{
    lv_obj_t* overlay = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
    bsp_display_backlight_on();
    if (overlay != nullptr) {
        lv_obj_del_async(overlay);
    }
}
}  // namespace

// Light sleep: black the screen + kill the backlight; any touch wakes it.
extern "C" void spectra5_sleep()
{
    lv_obj_t* overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(overlay, &wake_on_touch, LV_EVENT_CLICKED, overlay);
    bsp_display_backlight_off();
}

// Hard power off via the PMIC poweroff signal.
extern "C" void spectra5_power_off()
{
    bsp_generate_poweroff_signal();
}

#include "ui/screens/settings_screen.hpp"

#include <cstdio>
#include <ctime>
#include <string>

#include "core/version.hpp"
#include "hal/hal.h"
#include "services/system/system_service.hpp"
#include "ui/design_system/lv_color.hpp"

// True when the M5 keyboard is connected (skip the on-screen keyboard then).
extern "C" bool spectra5_keyboard_connected();
// Keypad-indev navigation group (the physical keyboard routes through it).
extern "C" lv_group_t* spectra5_nav_group();

namespace spectra5::ui {

using tokens::SemanticColor;

namespace {

struct ThemeClickCtx {
    SettingsScreen* self;
    ThemeId theme;
};

lv_obj_t* make_card(lv_obj_t* parent, const char* title)
{
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, lv_semantic(SemanticColor::SurfaceRaised), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, tokens::RadiusMd, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_semantic(SemanticColor::Border), 0);
    lv_obj_set_style_pad_all(card, tokens::SpaceLg, 0);
    lv_obj_set_style_pad_row(card, tokens::SpaceMd, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(card);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, lv_semantic(SemanticColor::TextSecondary), 0);
    lv_obj_set_style_text_font(lbl, &ibm_plex_mono_16, 0);
    return card;
}

lv_obj_t* make_info_row(lv_obj_t* parent, const std::string& text)
{
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text.c_str());
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, lv_pct(100));
    lv_obj_set_style_text_color(lbl, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(lbl, &ibm_plex_mono_18, 0);
    return lbl;
}

}  // namespace

SettingsScreen::SettingsScreen(lv_obj_t* parent, ThemeChanged on_theme_changed)
    : on_theme_changed_(std::move(on_theme_changed))
{
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, tokens::SpaceXl, 0);
    lv_obj_set_style_pad_row(root_, tokens::SpaceLg, 0);
    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* header = lv_label_create(root_);
    lv_label_set_text(header, LV_SYMBOL_SETTINGS "  Settings");
    lv_obj_set_style_text_color(header, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(header, &ibm_plex_mono_32, 0);

    /* ------------------------------- Appearance ------------------------------ */
    lv_obj_t* appearance = make_card(root_, "Appearance");

    lv_obj_t* theme_row = lv_obj_create(appearance);
    lv_obj_set_width(theme_row, lv_pct(100));
    lv_obj_set_height(theme_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(theme_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(theme_row, 0, 0);
    lv_obj_set_style_pad_all(theme_row, 0, 0);
    lv_obj_set_style_pad_column(theme_row, tokens::SpaceMd, 0);
    lv_obj_set_flex_flow(theme_row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(theme_row, LV_OBJ_FLAG_SCROLLABLE);

    struct ThemeOption {
        const char* label;
        ThemeId id;
    };
    const ThemeOption options[] = {{"Fett", ThemeId::Dark}, {"System", ThemeId::HighContrast}};
    const ThemeId current = active_theme().id();

    for (const auto& option : options) {
        const bool selected = option.id == current;

        lv_obj_t* btn = lv_obj_create(theme_row);
        lv_obj_set_size(btn, 220, tokens::TouchTarget);
        lv_obj_set_style_radius(btn, tokens::RadiusSm, 0);
        lv_obj_set_style_border_width(btn, selected ? 2 : 1, 0);
        lv_obj_set_style_border_color(
            btn, lv_semantic(selected ? SemanticColor::Accent : SemanticColor::Border), 0);
        lv_obj_set_style_bg_color(btn, lv_semantic(SemanticColor::Surface), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, option.label);
        lv_obj_set_style_text_color(
            lbl, lv_semantic(selected ? SemanticColor::Accent : SemanticColor::TextPrimary), 0);
        lv_obj_set_style_text_font(lbl, &ibm_plex_mono_18, 0);
        lv_obj_center(lbl);

        auto* ctx = new ThemeClickCtx{this, option.id};
        lv_obj_add_event_cb(btn, &SettingsScreen::on_theme_click, LV_EVENT_CLICKED, ctx);
        lv_obj_add_event_cb(
            btn, [](lv_event_t* e) { delete static_cast<ThemeClickCtx*>(lv_event_get_user_data(e)); },
            LV_EVENT_DELETE, ctx);
    }

    /* --------------------------- Display & Sound ----------------------------- */
    lv_obj_t* display = make_card(root_, "Display & Sound");

    auto styled_slider = [&](lv_obj_t* parent, int value, lv_event_cb_t cb) {
        lv_obj_t* s = lv_slider_create(parent);
        lv_obj_set_width(s, lv_pct(100));
        lv_slider_set_range(s, 0, 100);
        lv_slider_set_value(s, value, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(s, lv_semantic(SemanticColor::Border), LV_PART_MAIN);
        lv_obj_set_style_bg_color(s, lv_semantic(SemanticColor::Accent), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(s, lv_semantic(SemanticColor::Accent), LV_PART_KNOB);
        lv_obj_add_event_cb(s, cb, LV_EVENT_VALUE_CHANGED, this);
        return s;
    };

    brightness_lbl_ = lv_label_create(display);
    lv_obj_set_style_text_color(brightness_lbl_, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(brightness_lbl_, &ibm_plex_mono_18, 0);
    const uint8_t brightness = GetHAL()->getDisplayBrightness();
    char bbuf[32];
    std::snprintf(bbuf, sizeof(bbuf), "Brightness: %u%%", brightness);
    lv_label_set_text(brightness_lbl_, bbuf);
    styled_slider(display, brightness, &SettingsScreen::on_brightness_changed);

    volume_lbl_ = lv_label_create(display);
    lv_obj_set_style_text_color(volume_lbl_, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(volume_lbl_, &ibm_plex_mono_18, 0);
    const uint8_t volume = GetHAL()->getSpeakerVolume();
    char vbuf[32];
    std::snprintf(vbuf, sizeof(vbuf), "Volume: %u%%", volume);
    lv_label_set_text(volume_lbl_, vbuf);
    styled_slider(display, volume, &SettingsScreen::on_volume_changed);

    /* --------------------------------- Device -------------------------------- */
    lv_obj_t* device = make_card(root_, "Device");

    char rbuf[48];
    std::snprintf(rbuf, sizeof(rbuf), "Resolution: %dx%d", spectra5::kDisplayWidth, spectra5::kDisplayHeight);
    make_info_row(device, rbuf);
    make_info_row(device, std::string("Display panel: ") + GetHAL()->getDisplayPanelIc());
    make_info_row(device, std::string("Platform HAL: ") + GetHAL()->type());
    make_info_row(device, "Language: English");

    /* ------------------------------ Diagnostics ------------------------------ */
    lv_obj_t* diag = make_card(root_, "Diagnostics");
    if (auto* sys = services::system_service()) {
        const auto m = sys->metrics();
        char d[64];
        std::snprintf(d, sizeof(d), "C6 link: %s",
                      m.c6_status == services::CoprocessorStatus::Connected ? "OK" : "down");
        make_info_row(diag, d);
        std::snprintf(d, sizeof(d), "microSD: %s", m.sd_mounted ? "mounted" : "absent");
        make_info_row(diag, d);
        std::snprintf(d, sizeof(d), "Free heap: %u KB",
                      static_cast<unsigned>(m.free_heap_bytes / 1024u));
        make_info_row(diag, d);
        std::snprintf(d, sizeof(d), "CPU temp: %d C", m.cpu_temp_c);
        make_info_row(diag, d);
        std::snprintf(d, sizeof(d), "Battery: %d %%  (%.2f V)",
                      static_cast<int>(m.battery_percent + 0.5f), m.bus_voltage);
        make_info_row(diag, d);
        std::snprintf(d, sizeof(d), "Tasks running: %d", m.running_tasks);
        make_info_row(diag, d);
    } else {
        make_info_row(diag, "System metrics unavailable.");
    }

    /* ------------------------------ Date & Time ------------------------------ */
    lv_obj_t* dt = make_card(root_, "Date & Time");
    clock_lbl_   = lv_label_create(dt);
    lv_obj_set_style_text_color(clock_lbl_, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(clock_lbl_, &ibm_plex_mono_18, 0);
    {
        const std::time_t t = std::time(nullptr);
        std::tm tmv{};
        localtime_r(&t, &tmv);
        char c[48];
        std::snprintf(c, sizeof(c), "Now: %02d/%02d/%04d  %02d:%02d:%02d", tmv.tm_mday,
                      tmv.tm_mon + 1, tmv.tm_year + 1900, tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
        lv_label_set_text(clock_lbl_, c);
    }
    make_info_row(dt, "Stored in the battery-backed RTC -- survives reboots.");

    lv_obj_t* set_btn = lv_obj_create(dt);
    lv_obj_set_size(set_btn, 240, tokens::TouchTarget);
    lv_obj_set_style_radius(set_btn, tokens::RadiusSm, 0);
    lv_obj_set_style_border_width(set_btn, 1, 0);
    lv_obj_set_style_border_color(set_btn, lv_semantic(SemanticColor::Accent), 0);
    lv_obj_set_style_bg_color(set_btn, lv_semantic(SemanticColor::Surface), 0);
    lv_obj_set_style_bg_opa(set_btn, LV_OPA_COVER, 0);
    lv_obj_clear_flag(set_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* set_lbl = lv_label_create(set_btn);
    lv_label_set_text(set_lbl, LV_SYMBOL_EDIT "  Set date & time");
    lv_obj_set_style_text_color(set_lbl, lv_semantic(SemanticColor::Accent), 0);
    lv_obj_set_style_text_font(set_lbl, &ibm_plex_mono_18, 0);
    lv_obj_center(set_lbl);
    lv_obj_add_event_cb(set_btn, &SettingsScreen::on_set_time_click, LV_EVENT_CLICKED, this);
}

void SettingsScreen::on_set_time_click(lv_event_t* event)
{
    auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(event));
    if (self == nullptr) {
        return;
    }
    const std::time_t t = std::time(nullptr);
    std::tm tmv{};
    localtime_r(&t, &tmv);

    self->dt_modal_ = lv_obj_create(lv_layer_top());
    lv_obj_set_size(self->dt_modal_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(self->dt_modal_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(self->dt_modal_, LV_OPA_60, 0);
    lv_obj_set_style_border_width(self->dt_modal_, 0, 0);
    lv_obj_clear_flag(self->dt_modal_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(self->dt_modal_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(self->dt_modal_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* panel = lv_obj_create(self->dt_modal_);
    lv_obj_set_size(panel, 560, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(panel, lv_semantic(SemanticColor::Surface), 0);
    lv_obj_set_style_radius(panel, tokens::RadiusMd, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_semantic(SemanticColor::Accent), 0);
    lv_obj_set_style_pad_all(panel, tokens::SpaceLg, 0);
    lv_obj_set_style_pad_row(panel, tokens::SpaceMd, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(panel);
    lv_label_set_text(title, "Set date & time");
    lv_obj_set_style_text_color(title, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(title, &ibm_plex_mono_24, 0);
    lv_obj_t* hint = lv_label_create(panel);
    lv_label_set_text(hint, "Date  DD/MM/YYYY        Time  HH:MM:SS");
    lv_obj_set_style_text_color(hint, lv_semantic(SemanticColor::TextSecondary), 0);
    lv_obj_set_style_text_font(hint, &ibm_plex_mono_14, 0);

    char db[32];
    std::snprintf(db, sizeof(db), "%02d/%02d/%04d", tmv.tm_mday, tmv.tm_mon + 1, tmv.tm_year + 1900);
    self->date_ta_ = lv_textarea_create(panel);
    lv_textarea_set_one_line(self->date_ta_, true);
    lv_textarea_set_text(self->date_ta_, db);
    lv_obj_set_width(self->date_ta_, lv_pct(100));
    lv_obj_set_style_text_font(self->date_ta_, &ibm_plex_mono_18, 0);
    brand_input(self->date_ta_);

    char tb[24];
    std::snprintf(tb, sizeof(tb), "%02d:%02d:%02d", tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    self->time_ta_ = lv_textarea_create(panel);
    lv_textarea_set_one_line(self->time_ta_, true);
    lv_textarea_set_text(self->time_ta_, tb);
    lv_obj_set_width(self->time_ta_, lv_pct(100));
    lv_obj_set_style_text_font(self->time_ta_, &ibm_plex_mono_18, 0);
    brand_input(self->time_ta_);

    lv_obj_t* row = lv_obj_create(panel);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_column(row, tokens::SpaceMd, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    auto mk = [&](const char* txt, SemanticColor c, lv_event_cb_t cb) {
        lv_obj_t* b = lv_obj_create(row);
        lv_obj_set_size(b, 240, tokens::TouchTarget);
        lv_obj_set_style_radius(b, tokens::RadiusSm, 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_border_color(b, lv_semantic(c), 0);
        lv_obj_set_style_bg_color(b, lv_semantic(SemanticColor::SurfaceRaised), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, txt);
        lv_obj_set_style_text_color(l, lv_semantic(c), 0);
        lv_obj_set_style_text_font(l, &ibm_plex_mono_18, 0);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, self);
    };
    mk(LV_SYMBOL_CLOSE "  Cancel", SemanticColor::TextSecondary, &SettingsScreen::on_time_cancel);
    mk(LV_SYMBOL_SAVE "  Save", SemanticColor::Success, &SettingsScreen::on_time_save);

    // On-screen number keyboard only when there's no physical keyboard.
    if (!spectra5_keyboard_connected()) {
        lv_obj_t* kb = lv_keyboard_create(self->dt_modal_);
        lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
        lv_keyboard_set_textarea(kb, self->date_ta_);
        lv_obj_set_size(kb, lv_pct(100), lv_pct(40));
        // Focus the field a tap lands on -> route the number pad to it.
        lv_obj_add_event_cb(self->date_ta_,
                            [](lv_event_t* e) {
                                auto* k = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
                                lv_keyboard_set_textarea(k, static_cast<lv_obj_t*>(lv_event_get_target(e)));
                            },
                            LV_EVENT_FOCUSED, kb);
        lv_obj_add_event_cb(self->time_ta_,
                            [](lv_event_t* e) {
                                auto* k = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
                                lv_keyboard_set_textarea(k, static_cast<lv_obj_t*>(lv_event_get_target(e)));
                            },
                            LV_EVENT_FOCUSED, kb);
    }

    // Route the PHYSICAL keyboard into the fields: add them to the keypad group,
    // focus the first, enter editing. Tapping a field re-points the keyboard at it.
    if (lv_group_t* g = spectra5_nav_group()) {
        lv_group_add_obj(g, self->date_ta_);
        lv_group_add_obj(g, self->time_ta_);
        lv_group_focus_obj(self->date_ta_);
        lv_group_set_editing(g, true);
        auto refocus = [](lv_event_t* e) {
            if (lv_group_t* gg = spectra5_nav_group()) {
                lv_group_focus_obj(static_cast<lv_obj_t*>(lv_event_get_target(e)));
                lv_group_set_editing(gg, true);
            }
        };
        lv_obj_add_event_cb(self->date_ta_, refocus, LV_EVENT_CLICKED, nullptr);
        lv_obj_add_event_cb(self->time_ta_, refocus, LV_EVENT_CLICKED, nullptr);
    }
}

void SettingsScreen::on_time_cancel(lv_event_t* event)
{
    auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(event));
    if (self != nullptr && self->dt_modal_ != nullptr) {
        if (lv_group_t* g = spectra5_nav_group()) {
            lv_group_set_editing(g, false);  // un-stick keypad nav for the rail
        }
        lv_obj_del_async(self->dt_modal_);
        self->dt_modal_ = nullptr;
        self->date_ta_  = nullptr;
        self->time_ta_  = nullptr;
    }
}

void SettingsScreen::on_time_save(lv_event_t* event)
{
    auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(event));
    if (self == nullptr || self->date_ta_ == nullptr || self->time_ta_ == nullptr) {
        return;
    }
    int d = 0, mo = 0, y = 0, h = 0, mi = 0, s = 0;
    const char* ds = lv_textarea_get_text(self->date_ta_);
    const char* ts = lv_textarea_get_text(self->time_ta_);
    const int nd = std::sscanf(ds, "%d/%d/%d", &d, &mo, &y);
    const int nt = std::sscanf(ts, "%d:%d:%d", &h, &mi, &s);
    const bool ok = nd >= 3 && nt >= 2 && d >= 1 && d <= 31 && mo >= 1 && mo <= 12 && y >= 2020 &&
                    y <= 2099 && h >= 0 && h <= 23 && mi >= 0 && mi <= 59 && s >= 0 && s <= 59;
    if (ok) {
        std::tm tmv{};
        tmv.tm_mday = d;
        tmv.tm_mon  = mo - 1;
        tmv.tm_year = y - 1900;
        tmv.tm_hour = h;
        tmv.tm_min  = mi;
        tmv.tm_sec  = s;
        tmv.tm_isdst = -1;
        GetHAL()->setRtcTime(tmv);  // writes the battery-backed RTC + system time
        if (self->clock_lbl_ != nullptr) {
            char c[48];
            std::snprintf(c, sizeof(c), "Now: %02d/%02d/%04d  %02d:%02d:%02d", d, mo, y, h, mi, s);
            lv_label_set_text(self->clock_lbl_, c);
        }
    }
    if (self->dt_modal_ != nullptr) {
        if (lv_group_t* g = spectra5_nav_group()) {
            lv_group_set_editing(g, false);  // un-stick keypad nav for the rail
        }
        lv_obj_del_async(self->dt_modal_);
        self->dt_modal_ = nullptr;
        self->date_ta_  = nullptr;
        self->time_ta_  = nullptr;
    }
}

void SettingsScreen::on_theme_click(lv_event_t* event)
{
    auto* ctx = static_cast<ThemeClickCtx*>(lv_event_get_user_data(event));
    if (ctx == nullptr || ctx->self == nullptr) {
        return;
    }
    if (ctx->self->on_theme_changed_) {
        ctx->self->on_theme_changed_(ctx->theme);
    }
}

void SettingsScreen::on_brightness_changed(lv_event_t* event)
{
    auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(event));
    auto* slider = static_cast<lv_obj_t*>(lv_event_get_target(event));
    if (self == nullptr || slider == nullptr) {
        return;
    }

    const int value = lv_slider_get_value(slider);
    GetHAL()->setDisplayBrightness(static_cast<uint8_t>(value));

    if (self->brightness_lbl_ != nullptr) {
        char bbuf[32];
        std::snprintf(bbuf, sizeof(bbuf), "Brightness: %d%%", value);
        lv_label_set_text(self->brightness_lbl_, bbuf);
    }
}

void SettingsScreen::on_volume_changed(lv_event_t* event)
{
    auto* self   = static_cast<SettingsScreen*>(lv_event_get_user_data(event));
    auto* slider = static_cast<lv_obj_t*>(lv_event_get_target(event));
    if (self == nullptr || slider == nullptr) {
        return;
    }
    const int value = lv_slider_get_value(slider);
    GetHAL()->setSpeakerVolume(static_cast<uint8_t>(value));
    if (self->volume_lbl_ != nullptr) {
        char vbuf[32];
        std::snprintf(vbuf, sizeof(vbuf), "Volume: %d%%", value);
        lv_label_set_text(self->volume_lbl_, vbuf);
    }
}

}  // namespace spectra5::ui

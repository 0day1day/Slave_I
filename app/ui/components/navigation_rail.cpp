#include "ui/components/navigation_rail.hpp"

#include "ui/design_system/lv_color.hpp"

// Platform hooks (strong defs: about_overlay.cpp / power_control.cpp / hal_usb.cpp
// on device, desktop stubs otherwise). Declared only -- a weak def here would drop
// the device object.
extern "C" void spectra5_show_about();
extern "C" void spectra5_sleep();
extern "C" void spectra5_power_off();
extern "C" lv_group_t* spectra5_nav_group();

namespace spectra5::ui {

using tokens::SemanticColor;

namespace {
struct ClickCtx {
    NavigationModel* model;
    Route route;
};
struct FlyCtx {
    std::function<void()> action;
    lv_obj_t* modal;
};
}  // namespace

const char* route_icon(Route route)
{
    switch (route) {
        case Route::Dashboard:   return LV_SYMBOL_BARS;  // dashboard/overview, not a house
        case Route::Nearby:      return LV_SYMBOL_EYE_OPEN;
        case Route::Wifi:        return LV_SYMBOL_WIFI;
        case Route::Bluetooth:   return LV_SYMBOL_BLUETOOTH;
        case Route::Ieee802154:  return LV_SYMBOL_GPS;
        case Route::External:    return LV_SYMBOL_SHUFFLE;
        case Route::Rf:          return LV_SYMBOL_SHUFFLE;
        case Route::NfcRfid:     return LV_SYMBOL_LOOP;
        case Route::Infrared:    return LV_SYMBOL_VIDEO;
        case Route::Ports:       return LV_SYMBOL_USB;
        case Route::Sessions:    return LV_SYMBOL_SAVE;
        case Route::Workflows:   return LV_SYMBOL_REFRESH;
        case Route::Files:       return LV_SYMBOL_DIRECTORY;
        case Route::Diagnostics: return LV_SYMBOL_WARNING;
        case Route::Settings:    return LV_SYMBOL_SETTINGS;
    }
    return LV_SYMBOL_LIST;
}

NavigationRail::NavigationRail(lv_obj_t* parent, NavigationModel& model, const services::CapabilitySet& caps)
    : model_(model)
{
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, tokens::NavigationRailWidth, lv_pct(100));
    lv_obj_set_style_bg_color(root_, lv_semantic(SemanticColor::Surface), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_style_border_width(root_, 1, 0);
    lv_obj_set_style_border_side(root_, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_border_color(root_, lv_semantic(SemanticColor::Border), 0);
    lv_obj_set_style_pad_all(root_, tokens::SpaceXs, 0);
    lv_obj_set_style_pad_row(root_, tokens::SpaceXs, 0);
    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(root_, LV_SCROLLBAR_MODE_OFF);

    for (const auto& item : model_.items()) {
        const bool enabled = caps.has(item.required_capability);

        lv_obj_t* btn = lv_obj_create(root_);
        lv_obj_set_size(btn, lv_pct(100), 60);
        lv_obj_set_style_radius(btn, tokens::RadiusSm, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(btn, lv_semantic(SemanticColor::SurfaceRaised), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);
        // Keyboard focus ring (so arrow-key navigation is visible).
        lv_obj_set_style_bg_color(btn, lv_semantic(SemanticColor::SurfaceRaised), LV_STATE_FOCUSED);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_FOCUSED);
        lv_obj_set_style_outline_color(btn, lv_semantic(SemanticColor::Accent), LV_STATE_FOCUSED);
        lv_obj_set_style_outline_width(btn, 2, LV_STATE_FOCUSED);
        lv_obj_set_style_outline_pad(btn, -2, LV_STATE_FOCUSED);
        lv_obj_set_style_pad_all(btn, 2, 0);
        lv_obj_set_style_pad_row(btn, 2, 0);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* icon = lv_label_create(btn);
        lv_label_set_text(icon, route_icon(item.route));
        lv_obj_set_style_text_font(icon, &ibm_plex_mono_24, 0);
        lv_obj_set_style_text_color(
            icon, lv_semantic(enabled ? SemanticColor::TextSecondary : SemanticColor::Border), 0);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, item.label.c_str());
        lv_obj_set_style_text_font(lbl, &ibm_plex_mono_14, 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(
            lbl, lv_semantic(enabled ? SemanticColor::TextSecondary : SemanticColor::Border), 0);

        if (enabled && item.route == Route::External) {
            // External is a flyout trigger (RF / NFC / IR), not a destination.
            lv_obj_add_event_cb(
                btn,
                [](lv_event_t* e) {
                    auto* self = static_cast<NavigationRail*>(lv_event_get_user_data(e));
                    auto* anchor = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
                    self->show_flyout(
                        anchor,
                        {{LV_SYMBOL_SHUFFLE "  RF", [self] { self->model_.select(Route::Rf); }},
                         {LV_SYMBOL_LOOP "  NFC / RFID", [self] { self->model_.select(Route::NfcRfid); }},
                         {LV_SYMBOL_VIDEO "  Infrared", [self] { self->model_.select(Route::Infrared); }}});
                },
                LV_EVENT_CLICKED, this);
        } else if (enabled) {
            auto* ctx = new ClickCtx{&model_, item.route};
            lv_obj_add_event_cb(btn, &NavigationRail::on_click, LV_EVENT_CLICKED, ctx);
            lv_obj_add_event_cb(
                btn,
                [](lv_event_t* e) { delete static_cast<ClickCtx*>(lv_event_get_user_data(e)); },
                LV_EVENT_DELETE, ctx);
        } else {
            lv_obj_add_state(btn, LV_STATE_DISABLED);
        }

        if (enabled) {
            if (lv_group_t* g = spectra5_nav_group()) {
                lv_group_add_obj(g, btn);
            }
        }

        buttons_.push_back({item.route, btn, icon, lbl, enabled});
    }

    // Bottom-pinned "About" button (opens the crest overlay; not a route).
    lv_obj_t* spacer = lv_obj_create(root_);
    lv_obj_set_width(spacer, lv_pct(100));
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);

    // Bottom-pinned "Power" button -> flyout (Sleep / Power off; no destination).
    lv_obj_t* power = lv_obj_create(root_);
    lv_obj_set_size(power, lv_pct(100), 60);
    lv_obj_set_style_radius(power, tokens::RadiusSm, 0);
    lv_obj_set_style_border_width(power, 0, 0);
    lv_obj_set_style_bg_opa(power, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(power, lv_semantic(SemanticColor::SurfaceRaised), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(power, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_pad_all(power, 2, 0);
    lv_obj_set_flex_flow(power, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(power, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(power, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* picon = lv_label_create(power);
    lv_label_set_text(picon, LV_SYMBOL_POWER);
    lv_obj_set_style_text_font(picon, &ibm_plex_mono_24, 0);
    lv_obj_set_style_text_color(picon, lv_semantic(SemanticColor::Danger), 0);
    lv_obj_t* plbl = lv_label_create(power);
    lv_label_set_text(plbl, "Power");
    lv_obj_set_style_text_font(plbl, &ibm_plex_mono_14, 0);
    lv_obj_set_style_text_color(plbl, lv_semantic(SemanticColor::Danger), 0);
    lv_obj_add_event_cb(
        power,
        [](lv_event_t* e) {
            auto* self   = static_cast<NavigationRail*>(lv_event_get_user_data(e));
            auto* anchor = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
            self->show_flyout(anchor, {{LV_SYMBOL_EYE_CLOSE "  Sleep", [] { spectra5_sleep(); }},
                                       {LV_SYMBOL_POWER "  Power off", [] { spectra5_power_off(); }}});
        },
        LV_EVENT_CLICKED, this);
    if (lv_group_t* g = spectra5_nav_group()) {
        lv_group_add_obj(g, power);
    }

    lv_obj_t* about = lv_obj_create(root_);
    lv_obj_set_size(about, lv_pct(100), 60);
    lv_obj_set_style_radius(about, tokens::RadiusSm, 0);
    lv_obj_set_style_border_width(about, 0, 0);
    lv_obj_set_style_bg_opa(about, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(about, lv_semantic(SemanticColor::SurfaceRaised), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(about, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_pad_all(about, 2, 0);
    lv_obj_set_flex_flow(about, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(about, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(about, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* aicon = lv_label_create(about);
    lv_label_set_text(aicon, LV_SYMBOL_LIST);  // distinct from the 802.15.4 GPS glyph
    lv_obj_set_style_text_font(aicon, &ibm_plex_mono_24, 0);
    lv_obj_set_style_text_color(aicon, lv_semantic(SemanticColor::Accent), 0);
    lv_obj_t* albl = lv_label_create(about);
    lv_label_set_text(albl, "About");
    lv_obj_set_style_text_font(albl, &ibm_plex_mono_14, 0);
    lv_obj_set_style_text_color(albl, lv_semantic(SemanticColor::Accent), 0);
    lv_obj_add_event_cb(
        about,
        [](lv_event_t*) { spectra5_show_about(); },
        LV_EVENT_CLICKED, nullptr);
    if (lv_group_t* g = spectra5_nav_group()) {
        lv_group_add_obj(g, about);
        lv_group_focus_obj(buttons_.empty() ? about : buttons_.front().obj);
    }
    // About sits above Power in the rail.
    lv_obj_move_to_index(about, lv_obj_get_index(power));

    set_active(model_.current());
}

void NavigationRail::on_click(lv_event_t* event)
{
    auto* ctx = static_cast<ClickCtx*>(lv_event_get_user_data(event));
    if (ctx != nullptr && ctx->model != nullptr) {
        ctx->model->select(ctx->route);
    }
}

void NavigationRail::show_flyout(lv_obj_t* anchor, const std::vector<FlyItem>& items)
{
    lv_area_t a;
    lv_obj_get_coords(anchor, &a);

    // Full-screen transparent dismiss layer.
    lv_obj_t* modal = lv_obj_create(lv_layer_top());
    lv_obj_set_size(modal, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(modal, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(modal, 0, 0);
    lv_obj_set_style_pad_all(modal, 0, 0);
    lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(modal, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        modal, [](lv_event_t* e) { lv_obj_del_async(static_cast<lv_obj_t*>(lv_event_get_user_data(e))); },
        LV_EVENT_CLICKED, modal);

    lv_obj_t* panel = lv_obj_create(modal);
    lv_obj_set_width(panel, 230);
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(panel, lv_semantic(SemanticColor::Surface), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, tokens::RadiusMd, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_semantic(SemanticColor::Accent), 0);
    lv_obj_set_style_pad_all(panel, tokens::SpaceSm, 0);
    lv_obj_set_style_pad_row(panel, tokens::SpaceXs, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    // Position just right of the rail, vertically near the anchor (clamped on-screen).
    const int32_t vres = lv_display_get_vertical_resolution(lv_display_get_default());
    int32_t py         = a.y1;
    const int32_t maxy = vres - static_cast<int32_t>(items.size()) * 56 - 24;
    if (py > maxy) py = maxy < 8 ? 8 : maxy;
    lv_obj_set_pos(panel, a.x2 + 6, py);

    for (const auto& it : items) {
        auto* ctx = new FlyCtx{it.second, modal};
        lv_obj_t* b = lv_obj_create(panel);
        lv_obj_set_size(b, lv_pct(100), tokens::TouchTarget);
        lv_obj_set_style_radius(b, tokens::RadiusSm, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_style_bg_color(b, lv_semantic(SemanticColor::SurfaceRaised), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(b, lv_semantic(SemanticColor::Accent), LV_STATE_PRESSED);
        lv_obj_set_style_pad_hor(b, tokens::SpaceMd, 0);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, it.first.c_str());
        lv_obj_set_style_text_color(l, lv_semantic(SemanticColor::TextPrimary), 0);
        lv_obj_set_style_text_font(l, &ibm_plex_mono_18, 0);
        lv_obj_add_event_cb(
            b,
            [](lv_event_t* e) {
                auto* c = static_cast<FlyCtx*>(lv_event_get_user_data(e));
                if (c->action) c->action();
                lv_obj_del_async(c->modal);
            },
            LV_EVENT_CLICKED, ctx);
        lv_obj_add_event_cb(
            b, [](lv_event_t* e) { delete static_cast<FlyCtx*>(lv_event_get_user_data(e)); },
            LV_EVENT_DELETE, ctx);
    }
}

void NavigationRail::set_active(Route route)
{
    for (auto& button : buttons_) {
        const bool active = button.route == route;
        lv_obj_set_style_bg_opa(button.obj, active ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(button.obj, lv_semantic(SemanticColor::SurfaceRaised), 0);

        SemanticColor color = SemanticColor::TextSecondary;
        if (!button.enabled) {
            color = SemanticColor::Border;
        } else if (active) {
            color = SemanticColor::Accent;
        }
        lv_obj_set_style_text_color(button.icon, lv_semantic(color), 0);
        lv_obj_set_style_text_color(button.label, lv_semantic(color), 0);
    }
}

}  // namespace spectra5::ui

#include "ui/screens/module_overview_screen.hpp"

#include "ui/design_system/lv_color.hpp"

namespace spectra5::ui {

using tokens::SemanticColor;
using services::Capability;

namespace {

struct ModuleInfo {
    const char* description;
    const char* phase;
};

ModuleInfo module_info(Route route)
{
    switch (route) {
        case Route::Nearby:
            return {"Unified radar: every device seen across Wi-Fi + BLE in one live list, "
                    "sorted by signal. Aggregates the scans you already run.",
                    "PLANNED -- buildable now from the Wi-Fi + BLE scanners."};
        case Route::Wifi:
            return {"Access point and station scanning, channels, RSSI and timeline.", "LIVE."};
        case Route::Bluetooth:
            return {"BLE discovery, advertisements, vendor and signal strength.", "LIVE."};
        case Route::Ieee802154:
            return {"802.15.4 energy scan (Zigbee / Thread / Matter), channels 11-26.", "LIVE."};
        case Route::Rf:
            return {"Sub-GHz (CC1101) and 2.4 GHz NRF24. The Tab5 has no native sub-GHz radio "
                    "-- this needs the ULTRA expansion board. The PCB is designed; we'll produce "
                    "it and ship firmware support if the community wants it.",
                    "PLANNED -- ULTRA board (gauging community interest)."};
        case Route::NfcRfid:
            return {"NFC and 125 kHz RFID via the ULTRA expansion board (PCB designed). Built and "
                    "supported if there's community demand.",
                    "PLANNED -- ULTRA board (gauging community interest)."};
        case Route::Infrared:
            return {"Infrared capture and replay via the ULTRA expansion board (PCB designed). "
                    "Built and supported if there's community demand.",
                    "PLANNED -- ULTRA board (gauging community interest)."};
        case Route::Ports:
            return {"USB host: HID keyboards/mice and connected USB devices, plus UART/RS485 "
                    "over the Grove port.",
                    "PARTIAL -- USB host is up."};
        case Route::Sessions:
            return {"Saved capture sessions: scans, handshakes and observations recorded to SD "
                    "and re-openable / exportable.",
                    "LIVE."};
        case Route::Workflows:
            return {"Chain actions into one-tap automations (scan -> deauth -> capture).",
                    "PLANNED."};
        case Route::Files:
            return {"microSD browser: scans, handshakes, creds, probes under /sd/spectra5/.",
                    "LIVE."};
        case Route::Diagnostics:
            return {"System status, radio link health, logs and metrics -- folded into Settings.",
                    "LIVE -- see Settings."};
        case Route::Settings:
            return {"Theme, brightness, About. (Field Armor theme is active.)", "LIVE."};
        case Route::Dashboard:
        default:
            return {"General system overview.", "LIVE."};
    }
}

lv_obj_t* make_row_label(lv_obj_t* parent, const char* text, SemanticColor color, const lv_font_t* font)
{
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, lv_pct(100));
    lv_obj_set_style_text_color(lbl, lv_semantic(color), 0);
    lv_obj_set_style_text_font(lbl, font, 0);
    return lbl;
}

}  // namespace

ModuleOverviewScreen::ModuleOverviewScreen(lv_obj_t* parent, const NavItem& item,
                                           const services::CapabilitySet& caps)
{
    const bool available = caps.has(item.required_capability);
    const ModuleInfo info = module_info(item.route);

    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, tokens::SpaceXl, 0);
    lv_obj_set_style_pad_row(root_, tokens::SpaceMd, 0);
    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);

    // Cabecera con el nombre del módulo.
    make_row_label(root_, item.label.c_str(), SemanticColor::TextPrimary, &ibm_plex_mono_32);

    // Badge de disponibilidad.
    lv_obj_t* badge = lv_obj_create(root_);
    lv_obj_set_size(badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(badge, tokens::RadiusSm, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_pad_hor(badge, tokens::SpaceMd, 0);
    lv_obj_set_style_pad_ver(badge, tokens::SpaceXs, 0);
    lv_obj_set_style_bg_color(badge, lv_semantic(available ? SemanticColor::Success : SemanticColor::Warning), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_30, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* badge_lbl = lv_label_create(badge);
    lv_label_set_text(badge_lbl, available ? "Available" : "Unavailable");
    lv_obj_set_style_text_color(badge_lbl, lv_semantic(available ? SemanticColor::Success : SemanticColor::Warning), 0);
    lv_obj_set_style_text_font(badge_lbl, &ibm_plex_mono_16, 0);

    // Descripción.
    make_row_label(root_, info.description, SemanticColor::TextSecondary, &ibm_plex_mono_18);

    // Required hardware / capability.
    if (item.required_capability != Capability::None) {
        std::string hw = std::string("Required hardware: ") + services::capability_label(item.required_capability);
        make_row_label(root_, hw.c_str(), SemanticColor::TextSecondary, &ibm_plex_mono_16);
    }

    // Development phase.
    std::string phase = std::string("Status: ") + info.phase;
    make_row_label(root_, phase.c_str(), SemanticColor::Info, &ibm_plex_mono_16);

    // Enabled placeholder actions for the ToDo modules + a live status line.
    const char* b1 = nullptr;
    const char* b2 = nullptr;
    switch (item.route) {
        case Route::Rf:       b1 = LV_SYMBOL_REFRESH " Sub-GHz scan"; b2 = LV_SYMBOL_UP " NRF24 scan"; break;
        case Route::NfcRfid:  b1 = LV_SYMBOL_REFRESH " Read tag";     b2 = LV_SYMBOL_SAVE " Write tag"; break;
        case Route::Infrared: b1 = LV_SYMBOL_DOWNLOAD " Capture IR";  b2 = LV_SYMBOL_UP " Replay"; break;
        case Route::Nearby:   b1 = LV_SYMBOL_EYE_OPEN " Build radar"; break;
        case Route::Workflows:b1 = LV_SYMBOL_PLUS " New workflow"; break;
        case Route::Diagnostics: b1 = LV_SYMBOL_LIST " View logs"; break;
        case Route::Ports:    b1 = LV_SYMBOL_USB " List USB devices"; break;
        default: break;
    }
    if (b1 != nullptr) {
        lv_obj_t* note = lv_label_create(root_);
        lv_label_set_text(note, "");
        lv_obj_set_style_text_color(note, lv_semantic(SemanticColor::Warning), 0);
        lv_obj_set_style_text_font(note, &ibm_plex_mono_16, 0);

        lv_obj_t* row = lv_obj_create(root_);
        lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_pad_column(row, tokens::SpaceMd, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        auto add_btn = [&](const char* txt) {
            lv_obj_t* btn = lv_obj_create(row);
            lv_obj_set_size(btn, LV_SIZE_CONTENT, tokens::TouchTarget);
            lv_obj_set_style_radius(btn, tokens::RadiusSm, 0);
            lv_obj_set_style_border_width(btn, 1, 0);
            lv_obj_set_style_border_color(btn, lv_semantic(SemanticColor::Accent), 0);
            lv_obj_set_style_bg_color(btn, lv_semantic(SemanticColor::SurfaceRaised), 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_set_style_pad_hor(btn, tokens::SpaceMd, 0);
            lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_t* l = lv_label_create(btn);
            lv_label_set_text(l, txt);
            lv_obj_set_style_text_color(l, lv_semantic(SemanticColor::Accent), 0);
            lv_obj_center(l);
            lv_obj_add_event_cb(
                btn,
                [](lv_event_t* e) {
                    auto* n = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
                    lv_label_set_text(n, LV_SYMBOL_WARNING
                                      " Not yet -- needs an external module / future release.");
                },
                LV_EVENT_CLICKED, note);
        };
        add_btn(b1);
        if (b2 != nullptr) {
            add_btn(b2);
        }
    }
}

}  // namespace spectra5::ui

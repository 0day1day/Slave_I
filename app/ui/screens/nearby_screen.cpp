#include "ui/screens/nearby_screen.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "services/radio/ble_scanner.hpp"
#include "services/radio/radio_coordinator.hpp"
#include "services/radio/wifi_scanner.hpp"
#include "ui/design_system/lv_color.hpp"

namespace spectra5::ui {

using tokens::SemanticColor;

namespace {

constexpr int kPhaseTicks = 6;  // ~6 s per band before handing the radio over

struct Row {
    bool wifi;
    int rssi;
    std::string name;
    std::string extra;
};

}  // namespace

NearbyScreen::NearbyScreen(lv_obj_t* parent)
{
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, tokens::SpaceXl, 0);
    lv_obj_set_style_pad_row(root_, tokens::SpaceSm, 0);
    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* title = lv_label_create(root_);
    lv_label_set_text(title, LV_SYMBOL_EYE_OPEN "  NEARBY  -  unified radar");
    lv_obj_set_style_text_color(title, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(title, &ibm_plex_mono_32, 0);

    phase_lbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(phase_lbl_, lv_semantic(SemanticColor::Info), 0);
    lv_obj_set_style_text_font(phase_lbl_, &ibm_plex_mono_16, 0);
    lv_label_set_text(phase_lbl_, "Merging Wi-Fi + BLE by signal...");

    list_host_ = lv_obj_create(root_);
    lv_obj_set_width(list_host_, lv_pct(100));
    lv_obj_set_flex_grow(list_host_, 1);
    lv_obj_set_style_bg_opa(list_host_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list_host_, 0, 0);
    lv_obj_set_style_pad_all(list_host_, 0, 0);
    lv_obj_set_style_pad_row(list_host_, tokens::SpaceXs, 0);
    lv_obj_set_flex_flow(list_host_, LV_FLEX_FLOW_COLUMN);

    // Start on Wi-Fi.
    if (auto* coord = services::radio_coordinator()) {
        coord->prepare_wifi_route();
    }
    if (auto* wifi = services::wifi_scanner()) {
        wifi->start();
    }
    wifi_phase_ = true;
    timer_      = lv_timer_create(&NearbyScreen::on_timer, 1000, this);
    rebuild_list();
}

NearbyScreen::~NearbyScreen()
{
    if (timer_ != nullptr) {
        lv_timer_del(timer_);
        timer_ = nullptr;
    }
    if (auto* wifi = services::wifi_scanner()) {
        wifi->stop();
    }
    if (auto* ble = services::ble_scanner()) {
        ble->stop();
    }
}

void NearbyScreen::toggle_phase()
{
    auto* coord = services::radio_coordinator();
    auto* wifi  = services::wifi_scanner();
    auto* ble   = services::ble_scanner();
    if (wifi_phase_) {
        if (wifi) wifi->stop();
        if (coord) coord->prepare_ble_route();
        if (ble) ble->start();
        wifi_phase_ = false;
    } else {
        if (ble) ble->stop();
        if (coord) coord->prepare_wifi_route();
        if (wifi) wifi->start();
        wifi_phase_ = true;
    }
}

void NearbyScreen::on_timer(lv_timer_t* timer)
{
    auto* self = static_cast<NearbyScreen*>(lv_timer_get_user_data(timer));
    if (self == nullptr) {
        return;
    }
    if (++self->tick_ % kPhaseTicks == 0) {
        self->toggle_phase();
    }
    self->rebuild_list();
}

void NearbyScreen::rebuild_list()
{
    std::vector<Row> rows;
    if (auto* wifi = services::wifi_scanner()) {
        for (const auto& ap : wifi->snapshot()) {
            char ex[48];
            std::snprintf(ex, sizeof(ex), "Wi-Fi  ch %d", ap.channel);
            rows.push_back({true, ap.rssi, ap.ssid.empty() ? "<hidden>" : ap.ssid, ex});
        }
    }
    if (auto* ble = services::ble_scanner()) {
        for (const auto& d : ble->snapshot()) {
            std::string nm = !d.name.empty() ? d.name : (!d.vendor.empty() ? d.vendor : d.address);
            rows.push_back({false, d.rssi, nm, d.vendor.empty() ? "BLE" : ("BLE  " + d.vendor)});
        }
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.rssi > b.rssi; });

    if (phase_lbl_ != nullptr) {
        char p[64];
        std::snprintf(p, sizeof(p), "%s scanning  -  %d devices nearby",
                      wifi_phase_ ? "Wi-Fi" : "BLE", static_cast<int>(rows.size()));
        lv_label_set_text(phase_lbl_, p);
    }

    lv_obj_clean(list_host_);
    if (rows.empty()) {
        lv_obj_t* e = lv_label_create(list_host_);
        lv_label_set_text(e, "Listening... move around to populate the radar.");
        lv_obj_set_style_text_color(e, lv_semantic(SemanticColor::TextSecondary), 0);
        return;
    }

    for (const auto& r : rows) {
        lv_obj_t* row = lv_obj_create(list_host_);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, tokens::TouchTarget);
        lv_obj_set_style_bg_color(row, lv_semantic(SemanticColor::SurfaceRaised), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, tokens::RadiusSm, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_hor(row, tokens::SpaceMd, 0);
        lv_obj_set_style_pad_column(row, tokens::SpaceMd, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* icon = lv_label_create(row);
        lv_label_set_text(icon, r.wifi ? LV_SYMBOL_WIFI : LV_SYMBOL_BLUETOOTH);
        lv_obj_set_style_text_color(
            icon, lv_semantic(r.wifi ? SemanticColor::RadioWifi : SemanticColor::RadioBle), 0);

        lv_obj_t* name = lv_label_create(row);
        lv_label_set_text(name, r.name.c_str());
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_flex_grow(name, 1);
        lv_obj_set_style_text_color(name, lv_semantic(SemanticColor::TextPrimary), 0);
        lv_obj_set_style_text_font(name, &ibm_plex_mono_18, 0);

        lv_obj_t* extra = lv_label_create(row);
        lv_label_set_text(extra, r.extra.c_str());
        lv_obj_set_style_text_color(extra, lv_semantic(SemanticColor::TextSecondary), 0);
        lv_obj_set_style_text_font(extra, &ibm_plex_mono_14, 0);

        char rb[16];
        std::snprintf(rb, sizeof(rb), "%d dBm", r.rssi);
        lv_obj_t* rssi = lv_label_create(row);
        lv_label_set_text(rssi, rb);
        lv_obj_set_style_text_color(
            rssi, lv_semantic(r.rssi > -60 ? SemanticColor::Success : SemanticColor::TextSecondary),
            0);
        lv_obj_set_style_text_font(rssi, &ibm_plex_mono_16, 0);
    }
}

}  // namespace spectra5::ui

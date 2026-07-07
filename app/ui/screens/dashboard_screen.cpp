#include "ui/screens/dashboard_screen.hpp"

#include <array>
#include <cstdio>

#include "services/radio/evil_portal_service.hpp"
#include "services/stats/activity_stats.hpp"
#include "ui/design_system/lv_color.hpp"

namespace spectra5::ui {

using tokens::SemanticColor;
using namespace spectra5::services;

namespace {
const char* c6_caption(CoprocessorStatus s)
{
    switch (s) {
        case CoprocessorStatus::Connected:    return "Link active";
        case CoprocessorStatus::Disconnected: return "No link";
        case CoprocessorStatus::Error:        return "Link error";
        case CoprocessorStatus::Unknown:
        default:                              return "Unknown status";
    }
}
}  // namespace

DashboardScreen::DashboardScreen(lv_obj_t* parent)
{
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, tokens::SpaceLg, 0);
    lv_obj_set_style_pad_row(root_, tokens::SpaceMd, 0);
    lv_obj_set_style_pad_column(root_, tokens::SpaceMd, 0);
    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_ROW_WRAP);
    // Center the card grid horizontally so it doesn't hug the left edge.
    lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(root_, LV_DIR_VER);

    static const std::array<const char*, CardCount> titles = {
        "C6 Coprocessor", "microSD",        "Power",          "CPU Temp",
        "Free Memory",    "Radio Activity", "Devices",        "Tasks",
        "Deauths Sent",   "Beacons Sent",   "BLE Spam Runs",  "802.15.4 Scans",
        "Handshakes",     "Creds Captured",
    };

    cards_.reserve(CardCount);
    for (int i = 0; i < CardCount; ++i) {
        cards_.push_back(std::make_unique<MetricCard>(root_, titles[i]));
    }
}

void DashboardScreen::update(const services::SystemMetrics& m)
{
    char buf[48];

    cards_[CardC6]->set_value(m.c6_status == CoprocessorStatus::Connected ? "OK" : "--");
    cards_[CardC6]->set_caption(c6_caption(m.c6_status));
    cards_[CardC6]->set_accent(m.c6_status == CoprocessorStatus::Connected ? SemanticColor::Success
                                                                           : SemanticColor::TextSecondary);

    cards_[CardStorage]->set_value(m.sd_mounted ? "OK" : "--");
    cards_[CardStorage]->set_caption(m.sd_mounted ? "Mounted" : "Not mounted");
    cards_[CardStorage]->set_accent(m.sd_mounted ? SemanticColor::Success : SemanticColor::Warning);

    std::snprintf(buf, sizeof(buf), "%d %%", static_cast<int>(m.battery_percent + 0.5f));
    cards_[CardPower]->set_value(buf);
    std::snprintf(buf, sizeof(buf), "%.2f V  %.2f A", m.bus_voltage, m.bus_current);
    cards_[CardPower]->set_caption(buf);
    cards_[CardPower]->set_accent(m.battery_percent < 15.0f ? SemanticColor::Danger : SemanticColor::Accent);

    std::snprintf(buf, sizeof(buf), "%d C", m.cpu_temp_c);
    cards_[CardTemp]->set_value(buf);
    cards_[CardTemp]->set_caption("Internal sensor");
    cards_[CardTemp]->set_accent(m.cpu_temp_c > 70 ? SemanticColor::Danger : SemanticColor::Info);

    std::snprintf(buf, sizeof(buf), "%u MB", static_cast<unsigned>(m.free_heap_bytes / (1024u * 1024u)));
    cards_[CardMemory]->set_value(buf);
    if (m.total_heap_bytes > 0) {
        std::snprintf(buf, sizeof(buf), "of %u MB", static_cast<unsigned>(m.total_heap_bytes / (1024u * 1024u)));
        cards_[CardMemory]->set_caption(buf);
    } else {
        cards_[CardMemory]->set_caption("");
    }
    cards_[CardMemory]->set_accent(SemanticColor::Info);

    const bool scanning = m.radio_activity == RadioActivity::Scanning;
    cards_[CardRadio]->set_value(scanning ? "Scanning" : "Idle");
    cards_[CardRadio]->set_caption(scanning ? "Radio in use" : "No scan");
    cards_[CardRadio]->set_accent(scanning ? SemanticColor::Accent : SemanticColor::TextSecondary);

    std::snprintf(buf, sizeof(buf), "%d", m.observed_devices);
    cards_[CardDevices]->set_value(buf);
    cards_[CardDevices]->set_caption("Wi-Fi + BLE seen");
    cards_[CardDevices]->set_accent(m.observed_devices > 0 ? SemanticColor::RadioWifi
                                                           : SemanticColor::TextSecondary);

    std::snprintf(buf, sizeof(buf), "%d", m.running_tasks);
    cards_[CardTasks]->set_value(buf);
    cards_[CardTasks]->set_caption("FreeRTOS");
    cards_[CardTasks]->set_accent(SemanticColor::Accent);

    // --- tool-activity KPIs (cumulative this session) ---
    auto& a = ActivityStats::instance();
    auto kpi = [&](Card c, std::uint32_t v, const char* cap, SemanticColor accent) {
        std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(v));
        cards_[c]->set_value(buf);
        cards_[c]->set_caption(cap);
        cards_[c]->set_accent(v > 0 ? accent : SemanticColor::TextSecondary);
    };
    kpi(CardDeauths, a.deauth_frames.load(), "frames sent", SemanticColor::Danger);
    kpi(CardBeacons, a.beacon_frames.load(), "frames sent", SemanticColor::RadioWifi);
    kpi(CardBleSpam, a.ble_spam_sessions.load(), "runs launched", SemanticColor::RadioBle);
    kpi(CardZigbee, a.zigbee_scans.load(), "scans run", SemanticColor::RadioIeee802154);
    kpi(CardHandshakes, a.handshakes.load(), "captured", SemanticColor::Success);
    std::uint32_t creds = a.creds.load();
    if (services::has_evil_portal() && services::evil_portal() != nullptr) {
        const int c = services::evil_portal()->captured();
        if (c > 0 && static_cast<std::uint32_t>(c) > creds) {
            creds = static_cast<std::uint32_t>(c);
        }
    }
    kpi(CardCreds, creds, "captured", SemanticColor::Warning);
}

}  // namespace spectra5::ui

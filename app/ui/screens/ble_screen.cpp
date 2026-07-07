#include "ui/screens/ble_screen.hpp"

#include <cstdio>
#include <ctime>
#include <string>

#include "application/sessions/session_service.hpp"
#include "domain/observations/observation.hpp"
#include "services/radio/ble_scanner.hpp"
#include "services/radio/ble_spammer.hpp"
#include "services/stats/activity_stats.hpp"
#include "services/storage/sd_logger.hpp"
#include "ui/design_system/lv_color.hpp"

#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace spectra5::ui {

using tokens::SemanticColor;
using namespace spectra5::domain;

namespace {

constexpr std::size_t kMaxSaved = 256;

#if defined(ESP_PLATFORM)
struct BleSaveJob {
    lv_obj_t* status = nullptr;
    std::vector<BleAdvertisement> devices;
    char message[96]{};
};

static void ble_save_status_async(void* user)
{
    auto* job = static_cast<BleSaveJob*>(user);
    if (job->status != nullptr && lv_obj_is_valid(job->status)) {
        lv_label_set_text(job->status, job->message);
    }
    delete job;
}

static void ble_save_task(void* arg)
{
    auto* job      = static_cast<BleSaveJob*>(arg);
    auto* sessions = application::session_service();
    if (sessions == nullptr) {
        std::snprintf(job->message, sizeof(job->message), "No storage: cannot save scan.");
        lv_async_call(ble_save_status_async, job);
        vTaskDelete(nullptr);
        return;
    }
    char name[48];
    const std::time_t t = std::time(nullptr);
    std::tm tm_buf{};
    localtime_r(&t, &tm_buf);
    std::strftime(name, sizeof(name), "BLE scan %H:%M:%S", &tm_buf);
    auto created = sessions->create(name);
    if (!created) {
        std::snprintf(job->message, sizeof(job->message), "Could not create session.");
        lv_async_call(ble_save_status_async, job);
        vTaskDelete(nullptr);
        return;
    }
    std::size_t saved = 0;
    for (const auto& d : job->devices) {
        if (saved >= kMaxSaved) {
            break;
        }
        MetadataMap meta;
        meta["name"] = d.name.empty() ? "<unknown>" : d.name;
        meta["addr_type"] = ble_address_type_name(d.address_type);
        sessions->record_observation(created.value().id, ObservationType::BleDevice, d.address,
                                     d.rssi, meta);
        ++saved;
    }
    std::snprintf(job->message, sizeof(job->message), "Saved %d devices to \"%s\".",
                  static_cast<int>(saved), name);
    lv_async_call(ble_save_status_async, job);
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

BleScreen::BleScreen(lv_obj_t* parent)
{
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, tokens::SpaceXl, 0);
    lv_obj_set_style_pad_row(root_, tokens::SpaceMd, 0);
    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* header = lv_obj_create(root_);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_pad_column(header, tokens::SpaceMd, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, LV_SYMBOL_BLUETOOTH "  Bluetooth");
    lv_obj_set_style_text_color(title, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(title, &ibm_plex_mono_32, 0);
    lv_obj_set_flex_grow(title, 1);

    lv_obj_t* scan_btn = make_button(header, LV_SYMBOL_REFRESH " Scan", SemanticColor::Accent);
    scan_lbl_          = lv_obj_get_child(scan_btn, 0);
    lv_obj_add_event_cb(scan_btn, &BleScreen::on_scan_clicked, LV_EVENT_CLICKED, this);
    lv_obj_t* save_btn = make_button(header, LV_SYMBOL_SAVE " Save", SemanticColor::Success);
    lv_obj_add_event_cb(save_btn, &BleScreen::on_save_clicked, LV_EVENT_CLICKED, this);

    if (services::has_ble_spammer()) {
        lv_obj_t* spam_btn = make_button(header, LV_SYMBOL_WARNING " BLE Spam", SemanticColor::Danger);
        spam_lbl_          = lv_obj_get_child(spam_btn, 0);
        lv_obj_add_event_cb(spam_btn, &BleScreen::on_spam_clicked, LV_EVENT_CLICKED, this);
        // Target picker: which platform's pairing popup to spam.
        lv_obj_t* dd = lv_dropdown_create(header);
        lv_dropdown_set_options(dd, "All\nApple\nSamsung\nGoogle\nWindows");
        lv_obj_set_width(dd, 150);
        brand_input(dd);
        lv_obj_add_event_cb(dd, &BleScreen::on_spam_target, LV_EVENT_VALUE_CHANGED, this);
    }

    metrics_ = lv_label_create(root_);
    lv_obj_set_style_text_color(metrics_, lv_semantic(SemanticColor::TextSecondary), 0);
    lv_obj_set_style_text_font(metrics_, &ibm_plex_mono_16, 0);

    status_ = lv_label_create(root_);
    lv_obj_set_style_text_color(status_, lv_semantic(SemanticColor::TextSecondary), 0);
    lv_obj_set_style_text_font(status_, &ibm_plex_mono_14, 0);

    body_ = lv_obj_create(root_);
    lv_obj_set_width(body_, lv_pct(100));
    lv_obj_set_flex_grow(body_, 1);
    lv_obj_set_style_bg_opa(body_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body_, 0, 0);
    lv_obj_set_style_pad_all(body_, 0, 0);

    if (services::ble_scanner() == nullptr) {
        set_status("BLE scanner unavailable on this platform.");
        return;
    }

    rebuild_list();
    timer_ = lv_timer_create(&BleScreen::on_timer, 1200, this);
    update_scan_button();
    set_status("Press Scan to start BLE discovery.");
}

BleScreen::~BleScreen()
{
    pause_timer();
    if (auto* scanner = services::ble_scanner()) {
        scanner->stop();
        scanner->release_radio();
    }
    if (timer_ != nullptr) {
        lv_timer_del(timer_);
        timer_ = nullptr;
    }
}

void BleScreen::set_status(const char* text) { lv_label_set_text(status_, text); }

void BleScreen::pause_timer()
{
    if (timer_ != nullptr) {
        lv_timer_pause(timer_);
    }
}

void BleScreen::resume_timer()
{
    if (timer_ != nullptr) {
        lv_timer_resume(timer_);
    }
}

void BleScreen::update_scan_button()
{
    if (scan_lbl_ == nullptr) {
        return;
    }
    auto* scanner = services::ble_scanner();
    if (scanner != nullptr && scanner->is_scanning()) {
        lv_label_set_text(scan_lbl_, LV_SYMBOL_STOP " Stop");
    } else {
        lv_label_set_text(scan_lbl_, LV_SYMBOL_REFRESH " Scan");
    }
}

void BleScreen::on_timer(lv_timer_t* timer)
{
    auto* self = static_cast<BleScreen*>(lv_timer_get_user_data(timer));
    if (self != nullptr) {
        self->update_scan_button();
        self->rebuild_list();
    }
}

void BleScreen::rebuild_list()
{
    auto* scanner = services::ble_scanner();
    if (scanner == nullptr) {
        return;
    }
    auto devices = scanner->snapshot();
    char metrics[96];
    std::snprintf(metrics, sizeof(metrics), "%s  -  %d devices",
                  scanner->is_scanning() ? "Scanning" : "Idle", static_cast<int>(devices.size()));
    lv_label_set_text(metrics_, metrics);

    std::string sig;
    sig.reserve(devices.size() * 48);
    for (const auto& d : devices) {
        sig += d.address;
        sig += '\x1e';
        sig += d.name;
        sig += '\x1e';
        sig += std::to_string(d.rssi);
        sig += '\x1f';
    }
    if (sig == last_sig_) {
        return;
    }
    last_sig_ = sig;

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
        lv_table_set_column_width(table_, 0, 560);  // Name
        lv_table_set_column_width(table_, 1, 360);   // Address
        lv_table_set_column_width(table_, 2, 180);   // RSSI
    }

    lv_table_set_column_count(table_, 3);
    lv_table_set_row_count(table_, devices.size() + 1);
    lv_table_set_cell_value(table_, 0, 0, "Name");
    lv_table_set_cell_value(table_, 0, 1, "Address");
    lv_table_set_cell_value(table_, 0, 2, "RSSI");

    for (std::size_t i = 0; i < devices.size(); ++i) {
        const auto& d = devices[i];
        std::string name = d.name.empty()
                               ? (d.vendor.empty() ? "<unknown>" : "[" + d.vendor + "]")
                               : d.name;
        if (!d.name.empty() && !d.vendor.empty()) {
            name += "  (" + d.vendor + ")";
        }
        lv_table_set_cell_value(table_, i + 1, 0, name.c_str());
        lv_table_set_cell_value(table_, i + 1, 1, d.address.c_str());
        lv_table_set_cell_value_fmt(table_, i + 1, 2, "%d dBm", d.rssi);
    }
}

void BleScreen::on_scan_clicked(lv_event_t* event)
{
    auto* self    = static_cast<BleScreen*>(lv_event_get_user_data(event));
    auto* scanner = services::ble_scanner();
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

void BleScreen::on_spam_target(lv_event_t* event)
{
    auto* spammer = services::ble_spammer();
    auto* dd      = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    if (spammer == nullptr || dd == nullptr) {
        return;
    }
    spammer->set_spam_mode(static_cast<int>(lv_dropdown_get_selected(dd)));  // 0 All..4 Windows
}

void BleScreen::on_spam_clicked(lv_event_t* event)
{
    auto* self    = static_cast<BleScreen*>(lv_event_get_user_data(event));
    auto* spammer = services::ble_spammer();
    if (self == nullptr || spammer == nullptr) {
        return;
    }
    if (spammer->is_spamming()) {
        spammer->stop_spam();
        if (self->spam_lbl_ != nullptr) {
            lv_label_set_text(self->spam_lbl_, LV_SYMBOL_WARNING " BLE Spam");
        }
        self->set_status("BLE spam stopped.");
    } else {
        spammer->start_spam();
        services::ActivityStats::instance().ble_spam_sessions.fetch_add(1);
        if (self->spam_lbl_ != nullptr) {
            lv_label_set_text(self->spam_lbl_, LV_SYMBOL_STOP " Stop Spam");
        }
        self->set_status("BLE spam ON: Apple pairing popups flooding nearby devices.");
    }
}

void BleScreen::on_save_clicked(lv_event_t* event)
{
    auto* self    = static_cast<BleScreen*>(lv_event_get_user_data(event));
    auto* scanner = services::ble_scanner();
    if (self == nullptr || scanner == nullptr) {
        return;
    }
    self->pause_timer();
    // Dump to /sd/spectra5/ble/scan.csv (structured log).
    {
        auto& sd = services::SdLogger::instance();
        sd.enqueue("ble/scan.csv", "# address,name,vendor,rssi");
        for (const auto& d : scanner->snapshot()) {
            char line[180];
            std::snprintf(line, sizeof(line), "%s,%s,%s,%d", d.address.c_str(),
                          d.name.empty() ? "<unknown>" : d.name.c_str(),
                          d.vendor.empty() ? "?" : d.vendor.c_str(), d.rssi);
            sd.enqueue("ble/scan.csv", line);
        }
        sd.flush();
    }
#if defined(ESP_PLATFORM)
    auto* job   = new BleSaveJob{};
    job->status = self->status_;
    job->devices = scanner->snapshot();
    self->set_status("Saving scan to session...");
    if (xTaskCreate(ble_save_task, "ble_save", 8192, job, 3, nullptr) != pdPASS) {
        delete job;
        self->set_status("Could not start save task.");
    }
#else
    self->set_status("Save requires storage (Tab5).");
#endif
    self->resume_timer();
}

}  // namespace spectra5::ui

#include "ui/app_shell.hpp"

#include "core/version.hpp"
#include "services/radio/ble_scanner.hpp"
#include "services/radio/radio_coordinator.hpp"
#include "services/radio/wifi_scanner.hpp"
#include "ui/design_system/lv_color.hpp"

// Platform boot splash. Defined by the device build (boot_splash.cpp) or the
// desktop stub. Declared (not weak-defined) so the linker keeps the strong
// device object instead of dropping it for a local weak no-op.
extern "C" void spectra5_show_boot_splash();
extern "C" lv_group_t* spectra5_nav_group();

namespace spectra5::ui {

namespace {

void handoff_radio_for_route(Route route)
{
    if (auto* coord = services::radio_coordinator()) {
        if (route == Route::Wifi) {
            coord->prepare_wifi_route();
        } else if (route == Route::Bluetooth) {
            coord->prepare_ble_route();
        } else {
            coord->release_all();
        }
        return;
    }

    auto* wifi = services::wifi_scanner();
    auto* ble  = services::ble_scanner();
    if (route == Route::Wifi) {
        if (ble != nullptr) {
            ble->stop();
            ble->release_radio();
        }
    } else if (route == Route::Bluetooth) {
        if (wifi != nullptr) {
            wifi->stop();
            wifi->release_radio();
        }
    } else {
        if (wifi != nullptr) {
            wifi->stop();
            wifi->release_radio();
        }
        if (ble != nullptr) {
            ble->stop();
            ble->release_radio();
        }
    }
}

}  // namespace

using tokens::SemanticColor;

AppShell::AppShell(lv_obj_t* parent, const services::CapabilitySet& caps) : parent_(parent), caps_(caps)
{
    // Raise the boot splash first so it covers the UI as it builds underneath
    // (instead of the UI flashing for a moment before the splash appears).
    spectra5_show_boot_splash();

    root_ = lv_obj_create(parent_);
    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_pad_row(root_, 0, 0);
    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    model_.add_observer([this](Route route) {
        if (nav_rail_) {
            nav_rail_->set_active(route);
        }
        show_route(route);
    });

    build();
    show_route(model_.current());
}

void AppShell::build()
{
    // Tint LVGL's built-in widget theme (switches, sliders, scrollbars) with our
    // palette. Textareas/dropdowns are additionally brand-painted at creation via
    // ui::brand_input() (the default theme keeps them a cool grey otherwise).
    if (lv_display_t* disp = lv_display_get_default()) {
        lv_theme_t* th = lv_theme_default_init(disp, lv_semantic(SemanticColor::Accent),
                                               lv_semantic(SemanticColor::Info),
                                               /*dark=*/true, LV_FONT_DEFAULT);
        lv_display_set_theme(disp, th);
    }

    // Drop previous widgets (used on theme rebuild).
    dashboard_.reset();
    module_screen_.reset();
    settings_.reset();
    status_bar_.reset();
    nav_rail_.reset();
    lv_obj_clean(root_);

    lv_obj_set_style_bg_color(root_, lv_semantic(SemanticColor::Background), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);

    status_bar_ = std::make_unique<StatusBar>(root_);

    lv_obj_t* middle = lv_obj_create(root_);
    lv_obj_set_width(middle, lv_pct(100));
    lv_obj_set_flex_grow(middle, 1);
    lv_obj_set_style_bg_opa(middle, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(middle, 0, 0);
    lv_obj_set_style_border_width(middle, 0, 0);
    lv_obj_set_style_pad_all(middle, 0, 0);
    lv_obj_set_style_pad_column(middle, 0, 0);
    lv_obj_set_flex_flow(middle, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(middle, LV_OBJ_FLAG_SCROLLABLE);

    nav_rail_ = std::make_unique<NavigationRail>(middle, model_, caps_);

    content_host_ = lv_obj_create(middle);
    lv_obj_set_height(content_host_, lv_pct(100));
    lv_obj_set_flex_grow(content_host_, 1);
    lv_obj_set_style_bg_color(content_host_, lv_semantic(SemanticColor::Background), 0);
    lv_obj_set_style_bg_opa(content_host_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(content_host_, 0, 0);
    lv_obj_set_style_border_width(content_host_, 0, 0);
    lv_obj_set_style_pad_all(content_host_, 0, 0);

    task_bar_ = lv_obj_create(root_);
    lv_obj_set_size(task_bar_, lv_pct(100), tokens::TaskBarHeight);
    lv_obj_set_style_bg_color(task_bar_, lv_semantic(SemanticColor::Surface), 0);
    lv_obj_set_style_bg_opa(task_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(task_bar_, 0, 0);
    lv_obj_set_style_border_width(task_bar_, 1, 0);
    lv_obj_set_style_border_side(task_bar_, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(task_bar_, lv_semantic(SemanticColor::Border), 0);
    lv_obj_set_style_pad_hor(task_bar_, tokens::SpaceMd, 0);
    lv_obj_clear_flag(task_bar_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* version = lv_label_create(task_bar_);
    lv_label_set_text(version, spectra5::kVersionLabel);
    lv_obj_set_style_text_color(version, lv_semantic(SemanticColor::TextSecondary), 0);
    lv_obj_set_style_text_font(version, &ibm_plex_mono_14, 0);
    lv_obj_align(version, LV_ALIGN_LEFT_MID, 0, 0);
}

void AppShell::rebuild()
{
    build();
    show_route(model_.current());
}

void AppShell::show_route(Route route)
{
    handoff_radio_for_route(route);

    dashboard_.reset();
    module_screen_.reset();
    settings_.reset();
    sessions_.reset();
    files_.reset();
    workflows_.reset();
    wifi_.reset();
    ble_.reset();
    zigbee_.reset();
    nearby_.reset();
    lv_obj_clean(content_host_);

    if (route == Route::Dashboard) {
        dashboard_ = std::make_unique<DashboardScreen>(content_host_);
        dashboard_->update(services::system_service()->metrics());
        return;
    }

    if (route == Route::Nearby) {
        nearby_ = std::make_unique<NearbyScreen>(content_host_);
        return;
    }

    if (route == Route::Sessions) {
        sessions_ = std::make_unique<SessionsScreen>(content_host_);
        return;
    }

    if (route == Route::Files) {
        files_ = std::make_unique<FilesScreen>(content_host_);
        return;
    }

    if (route == Route::Wifi) {
        wifi_ = std::make_unique<WifiScreen>(content_host_);
        return;
    }

    if (route == Route::Bluetooth) {
        ble_ = std::make_unique<BleScreen>(content_host_);
        return;
    }

    if (route == Route::Ieee802154) {
        zigbee_ = std::make_unique<ZigbeeScreen>(content_host_);
        return;
    }
    // Route::External is a sidebar flyout (RF/NFC/IR), not a destination.

    if (route == Route::Settings) {
        settings_ = std::make_unique<SettingsScreen>(content_host_, [this](ThemeId id) {
            set_active_theme(id);
            rebuild();
        });
        return;
    }

    // External-hardware modules are reached via the External flyout, so they are
    // not nav items -- synthesize a NavItem so their ToDo placeholder still shows.
    if (route == Route::Rf || route == Route::NfcRfid || route == Route::Infrared) {
        const char* label = route == Route::Rf ? "RF" : route == Route::NfcRfid ? "NFC / RFID"
                                                                                : "Infrared";
        NavItem item{route, label, services::Capability::None};
        module_screen_ = std::make_unique<ModuleOverviewScreen>(content_host_, item, caps_);
        return;
    }

    const NavItem* item = model_.find(route);
    if (item != nullptr) {
        module_screen_ = std::make_unique<ModuleOverviewScreen>(content_host_, *item, caps_);
    }
}

void AppShell::refresh(const services::SystemMetrics& metrics)
{
    if (status_bar_) {
        status_bar_->update(metrics);
    }
    if (dashboard_) {
        dashboard_->update(metrics);
    }
}

void AppShell::set_clock(const char* hhmm)
{
    if (status_bar_) {
        status_bar_->set_clock(hhmm);
    }
}

}  // namespace spectra5::ui

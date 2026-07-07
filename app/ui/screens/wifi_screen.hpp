#pragma once

#include <string>
#include <vector>

#include <lvgl.h>

#include "domain/radio/offensive.hpp"
#include "domain/radio/wifi.hpp"

namespace spectra5::ui {

// Wi-Fi module (PRD Phase 3): scan control, metrics, band filter, a table list
// of access points (handles the 500 AP scenario via lv_table) and a per-AP
// detail view with a signal timeline. Observations can be saved into a session.
class WifiScreen {
public:
    explicit WifiScreen(lv_obj_t* parent);
    ~WifiScreen();

private:
    void rebuild_list();
    void build_table();
    void build_channels();
    void show_detail(const domain::WifiAccessPoint& ap);
    void set_status(const char* text);
    void update_attack_console();
    void update_scan_button();
    void update_filter_styles();
    void prepare_body_layout();
    void pause_timer();
    void resume_timer();

    static void on_scan_clicked(lv_event_t* event);
    static void on_save_clicked(lv_event_t* event);
    static void on_filter_clicked(lv_event_t* event);
    static void on_view_clicked(lv_event_t* event);
    static void on_band_changed(lv_event_t* event);
    static void on_search_changed(lv_event_t* event);
    static void on_search_focus(lv_event_t* event);
    static void on_keyboard_done(lv_event_t* event);
    static void on_row_selected(lv_event_t* event);
    static void on_attack_open(lv_event_t* event);
    static void on_portal_open(lv_event_t* event);
    static void on_beacon_clicked(lv_event_t* event);
    static void on_beacon_timer(lv_timer_t* timer);
    static void on_spam_mode(lv_event_t* event);
    static void on_spam_toggle(lv_event_t* event);
    static void on_spam_close(lv_event_t* event);
    static void on_clone_spam(lv_event_t* event);
    static void on_evil_twin(lv_event_t* event);
    static void on_sae_toggle(lv_event_t* event);
    static void on_sae_timer(lv_timer_t* timer);
    static void on_verify_pwd(lv_event_t* event);
    static void on_twin_deauth_toggle(lv_event_t* event);
    static void on_twin_deauth_timer(lv_timer_t* timer);
    static void on_template_changed(lv_event_t* event);
    static void on_detect_toggle(lv_event_t* event);
    static void on_sniff_toggle(lv_event_t* event);
    void open_radio_monitor(int kind);  // 1 = deauth detector, 2 = live sniffer
    static void on_monitor_close(lv_event_t* event);
    static void on_monitor_timer(lv_timer_t* timer);
    static void on_sniff_channel(lv_event_t* event);
    void open_spam_modal();
    void close_spam_modal();
    void update_spam_modes();
    void start_beacon_spam();
    void stop_beacon_spam();
    static void on_scan_clients(lv_event_t* event);
    static void on_attack_close(lv_event_t* event);
    static void on_attack_toggle(lv_event_t* event);
    static void on_attack_timer(lv_timer_t* timer);
    static void on_station_deauth(lv_event_t* event);
    void open_attack_modal();
    void close_attack_modal();
    void send_deauth_once();
    void send_deauth_to(const domain::MacAddr& target);
    void rebuild_station_list();
    static void on_show_detail_async(void* user_data);
    static void on_back_clicked(lv_event_t* event);
    static void on_exit_detail_async(void* user_data);
    static void on_timer(lv_timer_t* timer);

    lv_obj_t* root_      = nullptr;
    lv_obj_t* header_    = nullptr;  // list chrome, hidden while in the AP target view
    lv_obj_t* filters_row_ = nullptr;
    lv_obj_t* search_row_  = nullptr;
    lv_obj_t* scan_btn_  = nullptr;
    lv_obj_t* save_btn_  = nullptr;
    lv_obj_t* spam_btn_  = nullptr;       // WiFi/beacon spam header button (label child)
    lv_timer_t* beacon_timer_ = nullptr;  // drives beacon-spam SSID rotation
    lv_obj_t* spam_modal_ = nullptr;      // WiFi spam config modal
    lv_obj_t* spam_mode_btns_[4]{};       // Funny / Random / Karma / SD-list selectors
    std::vector<std::string> sd_ssids_;   // SSIDs loaded from /sd/spectra5/ssids.txt
    int karma_hop_ = 0;                    // Karma capture channel-hop index (1/6/11)
    lv_obj_t* detect_lbl_  = nullptr;      // deauth-detector header button label
    bool detect_active_    = false;        // deauth detector running
    lv_obj_t* sniff_lbl_   = nullptr;      // sniffer header button label
    bool sniff_active_     = false;        // live sniffer running
    lv_obj_t* detail_console_  = nullptr;  // live C6 results in the AP target view
    uint32_t detail_console_rev_ = 0;
    lv_obj_t* monitor_modal_   = nullptr;  // detector/sniffer terminal window
    lv_obj_t* monitor_console_ = nullptr;
    lv_obj_t* monitor_stats_   = nullptr;
    lv_timer_t* monitor_timer_ = nullptr;
    int monitor_kind_          = 0;        // 1 detector, 2 sniffer
    uint32_t monitor_rev_      = 0;
    int sniff_channel_         = 0;        // 0 = all (hop 1-13), 1-13 = fixed channel
    int sniff_hop_             = 0;        // current hop index when sniff_channel_ == 0
    lv_obj_t* spam_status_  = nullptr;    // live counter inside the modal
    lv_obj_t* spam_console_ = nullptr;    // live C6 results (RadioConsole) in the modal
    uint32_t spam_console_rev_ = 0;
    int beacon_mode_       = 0;           // 0 funny list, 1 random, 2 clone scanned APs
    uint32_t beacon_count_ = 0;           // beacons emitted this session
    int beacon_warmup_     = 0;           // ticks to wait for the C6 radio hand-off to settle
    std::string clone_ssid_;              // non-empty: impersonate this single AP (from detail)
    std::string twin_ssid_;               // Evil Twin target SSID for the captive portal
    domain::MacAddr twin_bssid_{};        // Evil Twin real-AP BSSID (for optional deauth)
    uint8_t twin_channel_  = 1;           // Evil Twin real-AP channel
    lv_timer_t* twin_deauth_timer_ = nullptr;  // optional deauth of the real AP during twin
    std::vector<std::string> template_paths_;  // portal template dropdown -> file paths
    lv_timer_t* sae_timer_ = nullptr;     // WPA3 SAE commit-flood (DoS) timer
    lv_obj_t* sae_lbl_     = nullptr;     // WPA3 DoS button label
    domain::MacAddr sae_bssid_{};         // SAE flood target
    uint8_t sae_channel_   = 1;
    int sae_warmup_        = 0;           // ticks before halting scan + flooding (SDIO settle)
    lv_obj_t* scan_lbl_  = nullptr;
    lv_obj_t* band_btns_[3]{};      // legacy (unused since the band dropdown)
    lv_obj_t* channels_btn_ = nullptr;
    lv_obj_t* band_dd_      = nullptr;
    lv_obj_t* metrics_   = nullptr;
    lv_obj_t* status_    = nullptr;
    lv_obj_t* search_    = nullptr;
    lv_obj_t* keyboard_  = nullptr;
    lv_obj_t* body_      = nullptr;
    lv_obj_t* table_     = nullptr;
    lv_timer_t* timer_   = nullptr;

    // Attack modal (opened from the AP target view): on/off deauth + live console.
    lv_obj_t* attack_modal_   = nullptr;
    lv_obj_t* attack_console_ = nullptr;  // label inside the scrollable console box
    lv_obj_t* attack_box_     = nullptr;  // scroll container: console (deauth) or station list (monitor)
    lv_obj_t* attack_status_  = nullptr;  // status label inside the modal
    lv_timer_t* attack_timer_ = nullptr;  // re-sends deauth while the toggle is ON
    uint32_t console_rev_     = 0;        // last RadioConsole revision rendered
    uint32_t station_rev_     = 0;        // last StationStore revision rendered
    bool attack_active_       = false;
    bool monitor_mode_        = false;    // modal opened for client scan vs deauth
    bool portal_mode_         = false;    // modal opened for the Evil Portal
    bool targeting_           = false;    // continuous deauth on a specific client
    domain::MacAddr attack_target_{};     // the client being deauthed (monitor mode)

    int band_filter_     = -1;  // -1 all, 0 = 2.4 GHz, 1 = 5 GHz
    bool channels_view_  = false;
    int layout_mode_     = -1;  // -1 unknown, 0 table, 1 channels
    bool in_detail_      = false;
    std::string ssid_filter_;
    std::string last_sig_;
    std::vector<domain::WifiAccessPoint> filtered_;
    domain::WifiAccessPoint pending_detail_;  // copy captured for the deferred detail view
};

}  // namespace spectra5::ui

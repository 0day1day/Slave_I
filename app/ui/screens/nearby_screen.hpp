#pragma once

#include <lvgl.h>

namespace spectra5::ui {

// Nearby: unified radar that merges Wi-Fi APs + BLE devices into one live list
// sorted by signal. The Tab5 has a single shared C6 radio, so it time-multiplexes
// (a few seconds of Wi-Fi, then BLE) and keeps showing both stores' latest.
class NearbyScreen {
public:
    explicit NearbyScreen(lv_obj_t* parent);
    ~NearbyScreen();

private:
    static void on_timer(lv_timer_t* timer);
    void rebuild_list();
    void toggle_phase();

    lv_obj_t* root_      = nullptr;
    lv_obj_t* phase_lbl_ = nullptr;
    lv_obj_t* list_host_ = nullptr;
    lv_timer_t* timer_   = nullptr;
    int tick_            = 0;
    bool wifi_phase_     = true;
};

}  // namespace spectra5::ui

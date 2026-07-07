#pragma once

#include <cstdint>

#include <lvgl.h>

namespace spectra5::ui {

// 802.15.4 (Zigbee / Thread / Matter) energy scan: triggers a C6 sweep of channels
// 11-26 and shows the per-channel peak RSSI as bars. Its own nav destination.
class ZigbeeScreen {
public:
    explicit ZigbeeScreen(lv_obj_t* parent);
    ~ZigbeeScreen();

private:
    static void on_scan(lv_event_t* event);
    static void on_sniff_toggle(lv_event_t* event);
    static void on_chan_changed(lv_event_t* event);
    static void on_timer(lv_timer_t* timer);
    void rebuild_devices();
    void flush_pcap();

    lv_obj_t* root_      = nullptr;
    lv_obj_t* status_    = nullptr;
    lv_obj_t* results_   = nullptr;
    lv_obj_t* sniff_lbl_ = nullptr;
    lv_obj_t* chan_dd_   = nullptr;
    lv_timer_t* timer_   = nullptr;
    uint32_t rev_        = 0;
    uint32_t sniff_rev_  = 0;
    bool scanning_       = false;
    bool sniffing_       = false;
    uint8_t sniff_channel_ = 15;
};

}  // namespace spectra5::ui

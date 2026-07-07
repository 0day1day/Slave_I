#pragma once

#include <lvgl.h>
#include <string>

namespace spectra5::ui {

class BleScreen {
public:
    explicit BleScreen(lv_obj_t* parent);
    ~BleScreen();

private:
    void rebuild_list();
    void set_status(const char* text);
    void update_scan_button();
    void pause_timer();
    void resume_timer();

    static void on_scan_clicked(lv_event_t* event);
    static void on_save_clicked(lv_event_t* event);
    static void on_spam_clicked(lv_event_t* event);
    static void on_spam_target(lv_event_t* event);
    static void on_timer(lv_timer_t* timer);

    lv_obj_t* root_     = nullptr;
    lv_obj_t* scan_lbl_ = nullptr;
    lv_obj_t* spam_lbl_ = nullptr;
    lv_obj_t* metrics_  = nullptr;
    lv_obj_t* status_   = nullptr;
    lv_obj_t* body_     = nullptr;
    lv_obj_t* table_    = nullptr;
    lv_timer_t* timer_  = nullptr;
    std::string last_sig_;
};

}  // namespace spectra5::ui

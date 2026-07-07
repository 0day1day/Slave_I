#pragma once

#include <lvgl.h>

#include "services/system/system_service.hpp"

namespace spectra5::ui {

// Barra de estado superior (48 px). Muestra wordmark, estado del C6,
// microSD, batería y reloj. Se actualiza sin reconstruirse.
class StatusBar {
public:
    explicit StatusBar(lv_obj_t* parent);

    lv_obj_t* root() const { return root_; }

    void update(const services::SystemMetrics& metrics);
    void set_clock(const char* hhmm);

private:
    lv_obj_t* root_    = nullptr;
    lv_obj_t* c6_      = nullptr;
    lv_obj_t* sd_      = nullptr;
    lv_obj_t* battery_ = nullptr;
    lv_obj_t* clock_   = nullptr;
    lv_obj_t* kbd_     = nullptr;
};

}  // namespace spectra5::ui

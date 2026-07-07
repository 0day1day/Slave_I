#pragma once

#include <cstddef>

#include <lvgl.h>

namespace spectra5::ui {

// Workflows module: runs the built-in example workflow through the real
// WorkflowEngine, showing live progress and logs. Polls the engine snapshot via
// an LVGL timer so background execution is reflected in the UI.
class WorkflowsScreen {
public:
    explicit WorkflowsScreen(lv_obj_t* parent);
    ~WorkflowsScreen();

private:
    void refresh();

    static void on_run_clicked(lv_event_t* event);
    static void on_cancel_clicked(lv_event_t* event);
    static void on_timer(lv_timer_t* timer);

    lv_obj_t* root_      = nullptr;
    lv_obj_t* status_    = nullptr;
    lv_obj_t* log_host_  = nullptr;
    lv_timer_t* timer_   = nullptr;
    std::size_t last_log_size_ = 0;
};

}  // namespace spectra5::ui

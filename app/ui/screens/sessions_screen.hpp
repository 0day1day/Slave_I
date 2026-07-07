#pragma once

#include <string>

#include <lvgl.h>

namespace spectra5::ui {

// Sessions module screen wired to the real SessionService (filesystem-backed).
// Supports create, record observation, export JSONL, end/reopen and delete,
// satisfying the Phase 2 exit criteria end to end.
class SessionsScreen {
public:
    explicit SessionsScreen(lv_obj_t* parent);

    void build_list();
    void build_detail(const std::string& id);
    void set_status(const char* text);

    static void on_new_clicked(lv_event_t* event);
    static void on_row_action(lv_event_t* event);
    static void on_back_clicked(lv_event_t* event);

private:

    lv_obj_t* root_      = nullptr;
    lv_obj_t* status_    = nullptr;
    lv_obj_t* list_host_ = nullptr;
};

}  // namespace spectra5::ui

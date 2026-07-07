#pragma once

#include <string>

#include <lvgl.h>

namespace spectra5::ui {

// Files module: a browser + manager over the storage root (microSD on Tab5,
// local data dir on desktop) provided by services::FilesystemBrowser. Supports
// new folder/file, rename, delete, copy/cut/paste, with an on-screen keyboard
// for naming.
class FilesScreen {
public:
    explicit FilesScreen(lv_obj_t* parent);

    void navigate_to(const std::string& path);

private:
    enum class PendingOp { None, NewDir, NewFile, Rename };

    void build_list();
    void set_status(const char* text);
    void select_entry(const std::string& rel, bool is_dir, lv_obj_t* row);
    void open_name_dialog(PendingOp op, const char* initial);
    void close_name_dialog();
    void do_paste();
    std::string join(const std::string& name) const;  // cwd_ + "/" + name

    static void on_up_clicked(lv_event_t* event);
    static void on_entry_clicked(lv_event_t* event);   // navigate into folder
    static void on_row_select(lv_event_t* event);      // select an entry
    static void on_new_dir(lv_event_t* event);
    static void on_new_file(lv_event_t* event);
    static void on_rename(lv_event_t* event);
    static void on_delete(lv_event_t* event);
    static void on_copy(lv_event_t* event);
    static void on_cut(lv_event_t* event);
    static void on_paste(lv_event_t* event);
    static void on_name_ok(lv_event_t* event);
    static void on_name_cancel(lv_event_t* event);
    static void on_open(lv_event_t* event);
    static void on_file_save(lv_event_t* event);
    static void on_viewer_close(lv_event_t* event);
    void open_file_viewer(const std::string& rel);

    lv_obj_t* root_      = nullptr;
    lv_obj_t* path_lbl_  = nullptr;
    lv_obj_t* list_host_ = nullptr;
    lv_obj_t* status_lbl_ = nullptr;
    std::string cwd_;

    // Selection (relative path) + last selected row for highlighting.
    std::string selected_;
    bool selected_is_dir_ = false;
    lv_obj_t* selected_row_ = nullptr;

    // Clipboard for copy/cut/paste (relative path + cut flag).
    std::string clip_path_;
    bool clip_is_dir_ = false;
    bool clip_cut_    = false;

    // Name-entry dialog state.
    lv_obj_t* name_modal_ = nullptr;
    lv_obj_t* name_ta_    = nullptr;
    PendingOp pending_op_ = PendingOp::None;

    // File viewer/editor state.
    lv_obj_t* viewer_modal_ = nullptr;
    lv_obj_t* viewer_ta_    = nullptr;
    std::string viewer_path_;
};

}  // namespace spectra5::ui

#include "ui/screens/files_screen.hpp"

#include <cstdio>
#include <string>

#include "services/storage/filesystem_browser.hpp"
#include "ui/design_system/lv_color.hpp"

// Keyboard navigation group (HAL on device, weak null fallback in app_shell).
extern "C" lv_group_t* spectra5_nav_group();
extern "C" bool spectra5_keyboard_connected();

namespace spectra5::ui {

using tokens::SemanticColor;
using namespace spectra5::services;

namespace {

struct EntryCtx {
    FilesScreen* self;
    std::string path;
    bool is_dir;
    lv_obj_t* row;
};

std::string human_size(std::uint64_t bytes)
{
    char buf[32];
    if (bytes < 1024) {
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
    } else if (bytes < 1024 * 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
    }
    return buf;
}

std::string basename_of(const std::string& path)
{
    const auto slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

lv_obj_t* tool_btn(lv_obj_t* parent, const char* txt, SemanticColor color, lv_event_cb_t cb,
                   void* ud)
{
    lv_obj_t* b = lv_obj_create(parent);
    lv_obj_set_size(b, LV_SIZE_CONTENT, tokens::TouchTarget);
    lv_obj_set_style_radius(b, tokens::RadiusSm, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, lv_semantic(color), 0);
    lv_obj_set_style_bg_color(b, lv_semantic(SemanticColor::Surface), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(b, tokens::SpaceMd, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, lv_semantic(color), 0);
    lv_obj_set_style_text_font(l, &ibm_plex_mono_16, 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    return b;
}

}  // namespace

FilesScreen::FilesScreen(lv_obj_t* parent)
{
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, tokens::SpaceXl, 0);
    lv_obj_set_style_pad_row(root_, tokens::SpaceSm, 0);
    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);

    /* -------- Header: up + title -------- */
    lv_obj_t* header = lv_obj_create(root_);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_pad_column(header, tokens::SpaceMd, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* up = lv_obj_create(header);
    lv_obj_set_size(up, 56, tokens::TouchTarget);
    lv_obj_set_style_radius(up, tokens::RadiusSm, 0);
    lv_obj_set_style_border_width(up, 1, 0);
    lv_obj_set_style_border_color(up, lv_semantic(SemanticColor::Border), 0);
    lv_obj_set_style_bg_color(up, lv_semantic(SemanticColor::Surface), 0);
    lv_obj_set_style_bg_opa(up, LV_OPA_COVER, 0);
    lv_obj_clear_flag(up, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(up, &FilesScreen::on_up_clicked, LV_EVENT_CLICKED, this);
    lv_obj_t* up_lbl = lv_label_create(up);
    lv_label_set_text(up_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(up_lbl, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_center(up_lbl);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, LV_SYMBOL_DIRECTORY "  Files");
    lv_obj_set_style_text_color(title, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(title, &ibm_plex_mono_32, 0);

    /* -------- Toolbar -------- */
    lv_obj_t* bar = lv_obj_create(root_);
    lv_obj_set_width(bar, lv_pct(100));
    lv_obj_set_height(bar, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_pad_column(bar, tokens::SpaceSm, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    tool_btn(bar, LV_SYMBOL_DIRECTORY " New", SemanticColor::Accent, &FilesScreen::on_new_dir, this);
    tool_btn(bar, LV_SYMBOL_FILE " New", SemanticColor::Accent, &FilesScreen::on_new_file, this);
    tool_btn(bar, LV_SYMBOL_EYE_OPEN " Open", SemanticColor::Success, &FilesScreen::on_open, this);
    tool_btn(bar, LV_SYMBOL_EDIT " Rename", SemanticColor::Info, &FilesScreen::on_rename, this);
    tool_btn(bar, LV_SYMBOL_COPY " Copy", SemanticColor::Info, &FilesScreen::on_copy, this);
    tool_btn(bar, LV_SYMBOL_CUT " Cut", SemanticColor::Info, &FilesScreen::on_cut, this);
    tool_btn(bar, LV_SYMBOL_PASTE " Paste", SemanticColor::Success, &FilesScreen::on_paste, this);
    tool_btn(bar, LV_SYMBOL_TRASH " Delete", SemanticColor::Danger, &FilesScreen::on_delete, this);

    path_lbl_ = lv_label_create(root_);
    lv_label_set_long_mode(path_lbl_, LV_LABEL_LONG_DOT);
    lv_obj_set_width(path_lbl_, lv_pct(100));
    lv_obj_set_style_text_color(path_lbl_, lv_semantic(SemanticColor::TextSecondary), 0);
    lv_obj_set_style_text_font(path_lbl_, &ibm_plex_mono_14, 0);

    status_lbl_ = lv_label_create(root_);
    lv_label_set_long_mode(status_lbl_, LV_LABEL_LONG_DOT);
    lv_obj_set_width(status_lbl_, lv_pct(100));
    lv_label_set_text(status_lbl_, "Tap a file to select. Folders: tap the chevron to open.");
    lv_obj_set_style_text_color(status_lbl_, lv_semantic(SemanticColor::Info), 0);
    lv_obj_set_style_text_font(status_lbl_, &ibm_plex_mono_14, 0);

    list_host_ = lv_obj_create(root_);
    lv_obj_set_width(list_host_, lv_pct(100));
    lv_obj_set_flex_grow(list_host_, 1);
    lv_obj_set_style_bg_opa(list_host_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list_host_, 0, 0);
    lv_obj_set_style_pad_all(list_host_, 0, 0);
    lv_obj_set_style_pad_row(list_host_, tokens::SpaceXs, 0);
    lv_obj_set_flex_flow(list_host_, LV_FLEX_FLOW_COLUMN);

    build_list();
}

void FilesScreen::set_status(const char* text)
{
    if (status_lbl_ != nullptr) {
        lv_label_set_text(status_lbl_, text);
    }
}

std::string FilesScreen::join(const std::string& name) const
{
    return cwd_.empty() ? name : cwd_ + "/" + name;
}

void FilesScreen::navigate_to(const std::string& path)
{
    cwd_      = path;
    selected_.clear();
    selected_row_ = nullptr;
    build_list();
}

void FilesScreen::select_entry(const std::string& rel, bool is_dir, lv_obj_t* row)
{
    if (selected_row_ != nullptr) {
        lv_obj_set_style_border_width(selected_row_, 0, 0);
    }
    selected_        = rel;
    selected_is_dir_ = is_dir;
    selected_row_    = row;
    if (row != nullptr) {
        lv_obj_set_style_border_width(row, 2, 0);
        lv_obj_set_style_border_color(row, lv_semantic(SemanticColor::Accent), 0);
    }
    std::string s = "Selected: " + basename_of(rel);
    set_status(s.c_str());
}

void FilesScreen::build_list()
{
    auto* browser = storage_browser();
    std::string shown = "/" + cwd_;
    lv_label_set_text(path_lbl_, shown.c_str());
    selected_row_ = nullptr;

    if (browser == nullptr) {
        lv_obj_clean(list_host_);
        lv_obj_t* lbl = lv_label_create(list_host_);
        lv_label_set_text(lbl, "Storage unavailable on this platform.");
        lv_obj_set_style_text_color(lbl, lv_semantic(SemanticColor::TextSecondary), 0);
        return;
    }

    auto listed = browser->list(cwd_);
    lv_obj_clean(list_host_);
    if (!listed) {
        lv_obj_t* lbl = lv_label_create(list_host_);
        lv_label_set_text(lbl, "Cannot open this folder.");
        lv_obj_set_style_text_color(lbl, lv_semantic(SemanticColor::TextSecondary), 0);
        return;
    }

    auto& nodes = listed.value();
    if (nodes.empty()) {
        lv_obj_t* empty = lv_label_create(list_host_);
        lv_label_set_text(empty, "Empty folder.");
        lv_obj_set_style_text_color(empty, lv_semantic(SemanticColor::TextSecondary), 0);
        lv_obj_set_style_text_font(empty, &ibm_plex_mono_18, 0);
        return;
    }

    for (const auto& node : nodes) {
        const std::string rel = join(node.name);

        lv_obj_t* row = lv_obj_create(list_host_);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, tokens::TouchTarget);
        lv_obj_set_style_bg_color(row, lv_semantic(SemanticColor::SurfaceRaised), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, tokens::RadiusSm, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_hor(row, tokens::SpaceMd, 0);
        lv_obj_set_style_pad_column(row, tokens::SpaceMd, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* icon = lv_label_create(row);
        lv_label_set_text(icon, node.is_dir ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE);
        lv_obj_set_style_text_color(
            icon, lv_semantic(node.is_dir ? SemanticColor::Accent : SemanticColor::TextSecondary), 0);

        lv_obj_t* name = lv_label_create(row);
        lv_label_set_text(name, node.name.c_str());
        lv_obj_set_style_text_color(name, lv_semantic(SemanticColor::TextPrimary), 0);
        lv_obj_set_style_text_font(name, &ibm_plex_mono_18, 0);
        lv_obj_set_flex_grow(name, 1);

        if (!node.is_dir) {
            lv_obj_t* size = lv_label_create(row);
            lv_label_set_text(size, human_size(node.size).c_str());
            lv_obj_set_style_text_color(size, lv_semantic(SemanticColor::TextSecondary), 0);
            lv_obj_set_style_text_font(size, &ibm_plex_mono_14, 0);
        }

        // The row itself selects the entry (file or folder).
        auto* sctx = new EntryCtx{this, rel, node.is_dir, row};
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, &FilesScreen::on_row_select, LV_EVENT_CLICKED, sctx);
        lv_obj_add_event_cb(
            row, [](lv_event_t* e) { delete static_cast<EntryCtx*>(lv_event_get_user_data(e)); },
            LV_EVENT_DELETE, sctx);

        // Folders get a chevron button to navigate in.
        if (node.is_dir) {
            lv_obj_t* open = lv_obj_create(row);
            lv_obj_set_size(open, 48, tokens::TouchTarget - 8);
            lv_obj_set_style_radius(open, tokens::RadiusSm, 0);
            lv_obj_set_style_border_width(open, 1, 0);
            lv_obj_set_style_border_color(open, lv_semantic(SemanticColor::Border), 0);
            lv_obj_set_style_bg_opa(open, LV_OPA_TRANSP, 0);
            lv_obj_clear_flag(open, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_t* chev = lv_label_create(open);
            lv_label_set_text(chev, LV_SYMBOL_RIGHT);
            lv_obj_set_style_text_color(chev, lv_semantic(SemanticColor::Accent), 0);
            lv_obj_center(chev);
            auto* octx = new EntryCtx{this, rel, true, row};
            lv_obj_add_event_cb(open, &FilesScreen::on_entry_clicked, LV_EVENT_CLICKED, octx);
            lv_obj_add_event_cb(
                open, [](lv_event_t* e) { delete static_cast<EntryCtx*>(lv_event_get_user_data(e)); },
                LV_EVENT_DELETE, octx);
        }
    }
}

void FilesScreen::on_up_clicked(lv_event_t* event)
{
    auto* self = static_cast<FilesScreen*>(lv_event_get_user_data(event));
    if (self == nullptr || self->cwd_.empty()) {
        return;
    }
    const auto slash = self->cwd_.find_last_of('/');
    self->navigate_to((slash == std::string::npos) ? "" : self->cwd_.substr(0, slash));
}

void FilesScreen::on_entry_clicked(lv_event_t* event)
{
    auto* ctx = static_cast<EntryCtx*>(lv_event_get_user_data(event));
    if (ctx != nullptr && ctx->self != nullptr) {
        ctx->self->navigate_to(ctx->path);
    }
}

void FilesScreen::on_row_select(lv_event_t* event)
{
    auto* ctx = static_cast<EntryCtx*>(lv_event_get_user_data(event));
    if (ctx != nullptr && ctx->self != nullptr) {
        ctx->self->select_entry(ctx->path, ctx->is_dir, ctx->row);
    }
}

void FilesScreen::on_new_dir(lv_event_t* event)
{
    auto* self = static_cast<FilesScreen*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->open_name_dialog(PendingOp::NewDir, "new_folder");
    }
}

void FilesScreen::on_new_file(lv_event_t* event)
{
    auto* self = static_cast<FilesScreen*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->open_name_dialog(PendingOp::NewFile, "new_file.txt");
    }
}

void FilesScreen::on_rename(lv_event_t* event)
{
    auto* self = static_cast<FilesScreen*>(lv_event_get_user_data(event));
    if (self == nullptr) {
        return;
    }
    if (self->selected_.empty()) {
        self->set_status("Select an entry first, then Rename.");
        return;
    }
    self->open_name_dialog(PendingOp::Rename, basename_of(self->selected_).c_str());
}

void FilesScreen::on_delete(lv_event_t* event)
{
    auto* self = static_cast<FilesScreen*>(lv_event_get_user_data(event));
    if (self == nullptr) {
        return;
    }
    if (self->selected_.empty()) {
        self->set_status("Select an entry first, then Delete.");
        return;
    }
    auto* browser = storage_browser();
    if (browser != nullptr && browser->remove_path(self->selected_)) {
        self->set_status("Deleted.");
        self->selected_.clear();
        self->build_list();
    } else {
        self->set_status("Delete failed.");
    }
}

void FilesScreen::on_copy(lv_event_t* event)
{
    auto* self = static_cast<FilesScreen*>(lv_event_get_user_data(event));
    if (self == nullptr) {
        return;
    }
    if (self->selected_.empty()) {
        self->set_status("Select an entry first, then Copy.");
        return;
    }
    self->clip_path_   = self->selected_;
    self->clip_is_dir_ = self->selected_is_dir_;
    self->clip_cut_    = false;
    self->set_status(("Copied: " + basename_of(self->clip_path_)).c_str());
}

void FilesScreen::on_cut(lv_event_t* event)
{
    auto* self = static_cast<FilesScreen*>(lv_event_get_user_data(event));
    if (self == nullptr) {
        return;
    }
    if (self->selected_.empty()) {
        self->set_status("Select an entry first, then Cut.");
        return;
    }
    self->clip_path_   = self->selected_;
    self->clip_is_dir_ = self->selected_is_dir_;
    self->clip_cut_    = true;
    self->set_status(("Cut: " + basename_of(self->clip_path_)).c_str());
}

void FilesScreen::on_paste(lv_event_t* event)
{
    auto* self = static_cast<FilesScreen*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->do_paste();
    }
}

void FilesScreen::do_paste()
{
    if (clip_path_.empty()) {
        set_status("Clipboard is empty (Copy or Cut first).");
        return;
    }
    auto* browser = storage_browser();
    if (browser == nullptr) {
        return;
    }
    const std::string dst = join(basename_of(clip_path_));
    if (dst == clip_path_) {
        set_status("Source and destination are the same.");
        return;
    }
    bool ok = false;
    if (clip_cut_) {
        ok = browser->move_path(clip_path_, dst);
    } else {
        ok = browser->copy_path(clip_path_, dst);
    }
    if (ok) {
        set_status(clip_cut_ ? "Moved here." : "Pasted here.");
        if (clip_cut_) {
            clip_path_.clear();
        }
        build_list();
    } else {
        set_status("Paste failed.");
    }
}

void FilesScreen::open_name_dialog(PendingOp op, const char* initial)
{
    pending_op_ = op;
    name_modal_ = lv_obj_create(lv_layer_top());
    lv_obj_set_size(name_modal_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(name_modal_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(name_modal_, LV_OPA_50, 0);
    lv_obj_set_style_border_width(name_modal_, 0, 0);
    lv_obj_clear_flag(name_modal_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(name_modal_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(name_modal_, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    name_ta_ = lv_textarea_create(name_modal_);
    lv_textarea_set_one_line(name_ta_, true);
    lv_textarea_set_text(name_ta_, initial ? initial : "");
    lv_obj_set_width(name_ta_, lv_pct(80));
    lv_obj_set_style_text_font(name_ta_, &ibm_plex_mono_18, 0);
    brand_input(name_ta_);
    // Physical keyboard: Enter confirms, Esc cancels.
    lv_obj_add_event_cb(name_ta_, &FilesScreen::on_name_ok, LV_EVENT_READY, this);
    lv_obj_add_event_cb(name_ta_, &FilesScreen::on_name_cancel, LV_EVENT_CANCEL, this);

    // On-screen keyboard only when there's no physical keyboard attached.
    if (!spectra5_keyboard_connected()) {
        lv_obj_t* kb = lv_keyboard_create(name_modal_);
        lv_keyboard_set_textarea(kb, name_ta_);
        lv_obj_set_size(kb, lv_pct(100), lv_pct(50));
        lv_obj_add_event_cb(kb, &FilesScreen::on_name_ok, LV_EVENT_READY, this);
        lv_obj_add_event_cb(kb, &FilesScreen::on_name_cancel, LV_EVENT_CANCEL, this);
    }

    // Route the physical keyboard straight into the field.
    if (lv_group_t* g = spectra5_nav_group()) {
        lv_group_add_obj(g, name_ta_);
        lv_group_focus_obj(name_ta_);
        lv_group_set_editing(g, true);
    }
}

void FilesScreen::close_name_dialog()
{
    if (name_modal_ != nullptr) {
        lv_obj_del_async(name_modal_);
        name_modal_ = nullptr;
        name_ta_    = nullptr;
    }
    pending_op_ = PendingOp::None;
}

void FilesScreen::on_name_ok(lv_event_t* event)
{
    auto* self = static_cast<FilesScreen*>(lv_event_get_user_data(event));
    if (self == nullptr || self->name_ta_ == nullptr) {
        return;
    }
    const std::string name = lv_textarea_get_text(self->name_ta_);
    const PendingOp op      = self->pending_op_;
    const std::string sel   = self->selected_;
    self->close_name_dialog();
    if (name.empty()) {
        self->set_status("Name was empty.");
        return;
    }
    auto* browser = storage_browser();
    if (browser == nullptr) {
        return;
    }
    bool ok = false;
    switch (op) {
        case PendingOp::NewDir:  ok = browser->make_dir(self->join(name)); break;
        case PendingOp::NewFile: ok = browser->make_file(self->join(name)); break;
        case PendingOp::Rename:  ok = browser->move_path(sel, self->join(name)); break;
        case PendingOp::None:    break;
    }
    self->set_status(ok ? "Done." : "Operation failed.");
    if (ok) {
        self->selected_.clear();
        self->build_list();
    }
}

void FilesScreen::on_name_cancel(lv_event_t* event)
{
    auto* self = static_cast<FilesScreen*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->close_name_dialog();
    }
}

void FilesScreen::on_open(lv_event_t* event)
{
    auto* self = static_cast<FilesScreen*>(lv_event_get_user_data(event));
    if (self == nullptr) {
        return;
    }
    if (self->selected_.empty() || self->selected_is_dir_) {
        self->set_status("Select a file first, then Open.");
        return;
    }
    self->open_file_viewer(self->selected_);
}

void FilesScreen::open_file_viewer(const std::string& rel)
{
    auto* browser = storage_browser();
    if (browser == nullptr) {
        return;
    }
    auto content = browser->read_text(rel, 32 * 1024);
    viewer_path_ = rel;

    viewer_modal_ = lv_obj_create(lv_layer_top());
    lv_obj_set_size(viewer_modal_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(viewer_modal_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(viewer_modal_, LV_OPA_60, 0);
    lv_obj_set_style_border_width(viewer_modal_, 0, 0);
    lv_obj_clear_flag(viewer_modal_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(viewer_modal_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(viewer_modal_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* panel = lv_obj_create(viewer_modal_);
    lv_obj_set_size(panel, lv_pct(92), lv_pct(92));
    lv_obj_set_style_bg_color(panel, lv_semantic(SemanticColor::Surface), 0);
    lv_obj_set_style_radius(panel, tokens::RadiusMd, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_semantic(SemanticColor::Accent), 0);
    lv_obj_set_style_pad_all(panel, tokens::SpaceMd, 0);
    lv_obj_set_style_pad_row(panel, tokens::SpaceSm, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(panel);
    lv_label_set_text(title, (std::string(LV_SYMBOL_FILE "  ") + basename_of(rel)).c_str());
    lv_obj_set_style_text_color(title, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(title, &ibm_plex_mono_18, 0);

    viewer_ta_ = lv_textarea_create(panel);
    lv_obj_set_width(viewer_ta_, lv_pct(100));
    lv_obj_set_flex_grow(viewer_ta_, 1);
    lv_textarea_set_text(viewer_ta_, content ? content.value().c_str() : "(cannot read file)");
    lv_obj_set_style_text_font(viewer_ta_, &ibm_plex_mono_16, 0);
    brand_input(viewer_ta_);

    lv_obj_t* row = lv_obj_create(panel);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_column(row, tokens::SpaceMd, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    auto btn = [&](const char* txt, SemanticColor c, lv_event_cb_t cb) {
        lv_obj_t* b = lv_obj_create(row);
        lv_obj_set_size(b, 200, tokens::TouchTarget);
        lv_obj_set_style_radius(b, tokens::RadiusSm, 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_border_color(b, lv_semantic(c), 0);
        lv_obj_set_style_bg_color(b, lv_semantic(SemanticColor::SurfaceRaised), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, txt);
        lv_obj_set_style_text_color(l, lv_semantic(c), 0);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, this);
    };
    btn(LV_SYMBOL_SAVE "  Save", SemanticColor::Success, &FilesScreen::on_file_save);
    btn(LV_SYMBOL_CLOSE "  Close", SemanticColor::TextSecondary, &FilesScreen::on_viewer_close);

    // On-screen keyboard only when there's no physical keyboard attached.
    if (!spectra5_keyboard_connected()) {
        lv_obj_t* kb = lv_keyboard_create(panel);
        lv_keyboard_set_textarea(kb, viewer_ta_);
        lv_obj_set_size(kb, lv_pct(100), lv_pct(38));
    }
    if (lv_group_t* g = spectra5_nav_group()) {
        lv_group_add_obj(g, viewer_ta_);
        lv_group_focus_obj(viewer_ta_);
        lv_group_set_editing(g, true);
    }
}

void FilesScreen::on_file_save(lv_event_t* event)
{
    auto* self = static_cast<FilesScreen*>(lv_event_get_user_data(event));
    if (self == nullptr || self->viewer_ta_ == nullptr) {
        return;
    }
    auto* browser = storage_browser();
    const bool ok = browser != nullptr &&
                    browser->write_text(self->viewer_path_, lv_textarea_get_text(self->viewer_ta_));
    if (self->viewer_modal_ != nullptr) {
        lv_obj_del_async(self->viewer_modal_);
        self->viewer_modal_ = nullptr;
        self->viewer_ta_    = nullptr;
    }
    self->set_status(ok ? "Saved." : "Save failed.");
}

void FilesScreen::on_viewer_close(lv_event_t* event)
{
    auto* self = static_cast<FilesScreen*>(lv_event_get_user_data(event));
    if (self == nullptr) {
        return;
    }
    if (self->viewer_modal_ != nullptr) {
        lv_obj_del_async(self->viewer_modal_);
        self->viewer_modal_ = nullptr;
        self->viewer_ta_    = nullptr;
    }
}

}  // namespace spectra5::ui

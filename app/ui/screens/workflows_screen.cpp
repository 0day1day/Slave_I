#include "ui/screens/workflows_screen.hpp"

#include <cstdio>
#include <string>

#include "application/workflows/workflow_engine.hpp"
#include "ui/design_system/lv_color.hpp"

namespace spectra5::ui {

using tokens::SemanticColor;
using namespace spectra5::domain;

namespace {

lv_obj_t* make_button(lv_obj_t* parent, const char* text, SemanticColor border)
{
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, tokens::TouchTarget);
    lv_obj_set_style_radius(btn, tokens::RadiusSm, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_semantic(border), 0);
    lv_obj_set_style_bg_color(btn, lv_semantic(SemanticColor::Surface), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_semantic(SemanticColor::SurfaceRaised), LV_STATE_PRESSED);
    lv_obj_set_style_pad_hor(btn, tokens::SpaceLg, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(lbl, &ibm_plex_mono_18, 0);
    lv_obj_center(lbl);
    return btn;
}

SemanticColor status_color(WorkflowStatus s)
{
    switch (s) {
        case WorkflowStatus::Running:   return SemanticColor::Accent;
        case WorkflowStatus::Completed: return SemanticColor::Success;
        case WorkflowStatus::Failed:    return SemanticColor::Danger;
        case WorkflowStatus::Cancelled: return SemanticColor::Warning;
        case WorkflowStatus::Idle:
        default:                        return SemanticColor::TextSecondary;
    }
}

}  // namespace

WorkflowsScreen::WorkflowsScreen(lv_obj_t* parent)
{
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, tokens::SpaceXl, 0);
    lv_obj_set_style_pad_row(root_, tokens::SpaceMd, 0);
    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* title = lv_label_create(root_);
    lv_label_set_text(title, LV_SYMBOL_REFRESH "  Workflows");
    lv_obj_set_style_text_color(title, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(title, &ibm_plex_mono_32, 0);

    lv_obj_t* desc = lv_label_create(root_);
    lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(desc, lv_pct(100));
    lv_label_set_text(desc,
                      "Built-in workflow \"Passive Capture Demo\": create a session, record Wi-Fi "
                      "and BLE observations, wait, then export JSONL.");
    lv_obj_set_style_text_color(desc, lv_semantic(SemanticColor::TextSecondary), 0);
    lv_obj_set_style_text_font(desc, &ibm_plex_mono_16, 0);

    lv_obj_t* controls = lv_obj_create(root_);
    lv_obj_set_width(controls, lv_pct(100));
    lv_obj_set_height(controls, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(controls, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    lv_obj_set_style_pad_all(controls, 0, 0);
    lv_obj_set_style_pad_column(controls, tokens::SpaceMd, 0);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(controls, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* run = make_button(controls, LV_SYMBOL_PLAY " Run", SemanticColor::Success);
    lv_obj_add_event_cb(run, &WorkflowsScreen::on_run_clicked, LV_EVENT_CLICKED, this);
    lv_obj_t* cancel = make_button(controls, LV_SYMBOL_CLOSE " Cancel", SemanticColor::Danger);
    lv_obj_add_event_cb(cancel, &WorkflowsScreen::on_cancel_clicked, LV_EVENT_CLICKED, this);

    status_ = lv_label_create(root_);
    lv_obj_set_style_text_font(status_, &ibm_plex_mono_18, 0);

    log_host_ = lv_obj_create(root_);
    lv_obj_set_width(log_host_, lv_pct(100));
    lv_obj_set_flex_grow(log_host_, 1);
    lv_obj_set_style_bg_color(log_host_, lv_semantic(SemanticColor::Surface), 0);
    lv_obj_set_style_bg_opa(log_host_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(log_host_, tokens::RadiusMd, 0);
    lv_obj_set_style_border_width(log_host_, 1, 0);
    lv_obj_set_style_border_color(log_host_, lv_semantic(SemanticColor::Border), 0);
    lv_obj_set_style_pad_all(log_host_, tokens::SpaceMd, 0);
    lv_obj_set_style_pad_row(log_host_, tokens::SpaceXs, 0);
    lv_obj_set_flex_flow(log_host_, LV_FLEX_FLOW_COLUMN);

    if (application::workflow_engine() == nullptr) {
        lv_label_set_text(status_, "Workflow engine unavailable (no storage).");
        lv_obj_set_style_text_color(status_, lv_semantic(SemanticColor::Warning), 0);
    } else {
        refresh();
        timer_ = lv_timer_create(&WorkflowsScreen::on_timer, 300, this);
    }
}

WorkflowsScreen::~WorkflowsScreen()
{
    if (timer_ != nullptr) {
        lv_timer_del(timer_);
        timer_ = nullptr;
    }
}

void WorkflowsScreen::on_timer(lv_timer_t* timer)
{
    auto* self = static_cast<WorkflowsScreen*>(lv_timer_get_user_data(timer));
    if (self != nullptr) {
        self->refresh();
    }
}

void WorkflowsScreen::refresh()
{
    auto* engine = application::workflow_engine();
    if (engine == nullptr) {
        return;
    }

    const WorkflowRun run = engine->snapshot();

    char status[96];
    std::snprintf(status, sizeof(status), "Status: %s  (step %d/%d)", workflow_status_name(run.status),
                  static_cast<int>(run.current_step), static_cast<int>(run.total_steps));
    lv_label_set_text(status_, status);
    lv_obj_set_style_text_color(status_, lv_semantic(status_color(run.status)), 0);

    // Only rebuild the log when new lines appeared.
    if (run.log.size() == last_log_size_) {
        return;
    }
    last_log_size_ = run.log.size();
    lv_obj_clean(log_host_);
    for (const auto& line : run.log) {
        lv_obj_t* lbl = lv_label_create(log_host_);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lbl, lv_pct(100));
        lv_label_set_text(lbl, line.c_str());
        lv_obj_set_style_text_color(lbl, lv_semantic(SemanticColor::TextPrimary), 0);
        lv_obj_set_style_text_font(lbl, &ibm_plex_mono_14, 0);
    }
}

void WorkflowsScreen::on_run_clicked(lv_event_t* event)
{
    auto* self   = static_cast<WorkflowsScreen*>(lv_event_get_user_data(event));
    auto* engine = application::workflow_engine();
    if (self == nullptr || engine == nullptr) {
        return;
    }
    self->last_log_size_ = 0;
    Status st = engine->start(application::WorkflowEngine::example());
    if (!st) {
        lv_label_set_text(self->status_, st.error().message.c_str());
        lv_obj_set_style_text_color(self->status_, lv_semantic(SemanticColor::Warning), 0);
    }
}

void WorkflowsScreen::on_cancel_clicked(lv_event_t* event)
{
    auto* engine = application::workflow_engine();
    if (engine != nullptr) {
        engine->cancel();
    }
}

}  // namespace spectra5::ui

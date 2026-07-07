#include "ui/screens/sessions_screen.hpp"

#include <cstdio>
#include <string>

#include "application/sessions/session_service.hpp"
#include "ui/design_system/lv_color.hpp"

namespace spectra5::ui {

using tokens::SemanticColor;
using namespace spectra5::domain;

namespace {

enum class RowAction { View, AddObservation, Export, ToggleState, Delete };

struct RowCtx {
    SessionsScreen* self;
    SessionId id;
    RowAction action;
    SessionStatus status;
};

lv_obj_t* make_button(lv_obj_t* parent, const char* text, SemanticColor border)
{
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, 44);
    lv_obj_set_style_radius(btn, tokens::RadiusSm, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_semantic(border), 0);
    lv_obj_set_style_bg_color(btn, lv_semantic(SemanticColor::Surface), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_semantic(SemanticColor::SurfaceRaised), LV_STATE_PRESSED);
    lv_obj_set_style_pad_hor(btn, tokens::SpaceMd, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(lbl, &ibm_plex_mono_16, 0);
    lv_obj_center(lbl);
    return btn;
}

void attach_action(lv_obj_t* btn, SessionsScreen* self, const SessionId& id, RowAction action,
                   SessionStatus status)
{
    auto* ctx = new RowCtx{self, id, action, status};
    lv_obj_add_event_cb(btn, &SessionsScreen::on_row_action, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(
        btn, [](lv_event_t* e) { delete static_cast<RowCtx*>(lv_event_get_user_data(e)); },
        LV_EVENT_DELETE, ctx);
}

}  // namespace

SessionsScreen::SessionsScreen(lv_obj_t* parent)
{
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, tokens::SpaceXl, 0);
    lv_obj_set_style_pad_row(root_, tokens::SpaceMd, 0);
    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);

    // Header row: title + New Session.
    lv_obj_t* header = lv_obj_create(root_);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, LV_SYMBOL_SAVE "  Sessions");
    lv_obj_set_style_text_color(title, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(title, &ibm_plex_mono_32, 0);

    lv_obj_t* new_btn = make_button(header, LV_SYMBOL_PLUS "  New Session", SemanticColor::Accent);
    lv_obj_add_event_cb(new_btn, &SessionsScreen::on_new_clicked, LV_EVENT_CLICKED, this);

    status_ = lv_label_create(root_);
    lv_label_set_long_mode(status_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(status_, lv_pct(100));
    lv_obj_set_style_text_color(status_, lv_semantic(SemanticColor::TextSecondary), 0);
    lv_obj_set_style_text_font(status_, &ibm_plex_mono_14, 0);

    list_host_ = lv_obj_create(root_);
    lv_obj_set_width(list_host_, lv_pct(100));
    lv_obj_set_flex_grow(list_host_, 1);
    lv_obj_set_style_bg_opa(list_host_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list_host_, 0, 0);
    lv_obj_set_style_pad_all(list_host_, 0, 0);
    lv_obj_set_style_pad_row(list_host_, tokens::SpaceMd, 0);
    lv_obj_set_flex_flow(list_host_, LV_FLEX_FLOW_COLUMN);

    if (application::session_service() == nullptr) {
        set_status("Session service unavailable on this platform.");
    } else {
        build_list();
    }
}

void SessionsScreen::set_status(const char* text)
{
    if (status_ != nullptr) {
        lv_label_set_text(status_, text);
    }
}

void SessionsScreen::build_list()
{
    lv_obj_clean(list_host_);

    auto* service = application::session_service();
    if (service == nullptr) {
        return;
    }

    auto listed = service->list();
    if (!listed) {
        set_status("Could not read sessions.");
        return;
    }

    auto& sessions = listed.value();
    if (sessions.empty()) {
        lv_obj_t* empty = lv_label_create(list_host_);
        lv_label_set_text(empty, "No sessions yet. Tap \"New Session\" to create one.");
        lv_obj_set_style_text_color(empty, lv_semantic(SemanticColor::TextSecondary), 0);
        lv_obj_set_style_text_font(empty, &ibm_plex_mono_18, 0);
        return;
    }

    for (const auto& s : sessions) {
        lv_obj_t* card = lv_obj_create(list_host_);
        lv_obj_set_width(card, lv_pct(100));
        lv_obj_set_height(card, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(card, lv_semantic(SemanticColor::SurfaceRaised), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(card, tokens::RadiusMd, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_semantic(SemanticColor::Border), 0);
        lv_obj_set_style_pad_all(card, tokens::SpaceLg, 0);
        lv_obj_set_style_pad_row(card, tokens::SpaceSm, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* name = lv_label_create(card);
        lv_label_set_text(name, s.name.c_str());
        lv_obj_set_style_text_color(name, lv_semantic(SemanticColor::TextPrimary), 0);
        lv_obj_set_style_text_font(name, &ibm_plex_mono_20, 0);

        const bool ended = s.status == SessionStatus::Ended;
        char meta[96];
        std::snprintf(meta, sizeof(meta), "%s  -  %d observations", session_status_name(s.status),
                      s.observation_count);
        lv_obj_t* meta_lbl = lv_label_create(card);
        lv_label_set_text(meta_lbl, meta);
        lv_obj_set_style_text_color(
            meta_lbl, lv_semantic(ended ? SemanticColor::TextSecondary : SemanticColor::Success), 0);
        lv_obj_set_style_text_font(meta_lbl, &ibm_plex_mono_14, 0);

        lv_obj_t* actions = lv_obj_create(card);
        lv_obj_set_width(actions, lv_pct(100));
        lv_obj_set_height(actions, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(actions, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(actions, 0, 0);
        lv_obj_set_style_pad_all(actions, 0, 0);
        lv_obj_set_style_pad_column(actions, tokens::SpaceSm, 0);
        lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_clear_flag(actions, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* view_btn = make_button(actions, LV_SYMBOL_EYE_OPEN " View", SemanticColor::Info);
        attach_action(view_btn, this, s.id, RowAction::View, s.status);

        lv_obj_t* add_btn = make_button(actions, "Add observation", SemanticColor::Accent);
        attach_action(add_btn, this, s.id, RowAction::AddObservation, s.status);

        lv_obj_t* export_btn = make_button(actions, LV_SYMBOL_DOWNLOAD " Export", SemanticColor::Info);
        attach_action(export_btn, this, s.id, RowAction::Export, s.status);

        lv_obj_t* state_btn =
            make_button(actions, ended ? "Reopen" : "End", SemanticColor::Warning);
        attach_action(state_btn, this, s.id, RowAction::ToggleState, s.status);

        lv_obj_t* del_btn = make_button(actions, LV_SYMBOL_TRASH " Delete", SemanticColor::Danger);
        attach_action(del_btn, this, s.id, RowAction::Delete, s.status);
    }
}

void SessionsScreen::build_detail(const std::string& id)
{
    auto* service = application::session_service();
    if (service == nullptr) {
        return;
    }
    lv_obj_clean(list_host_);

    lv_obj_t* back = make_button(list_host_, LV_SYMBOL_LEFT " Back to sessions", SemanticColor::Border);
    lv_obj_add_event_cb(back, &SessionsScreen::on_back_clicked, LV_EVENT_CLICKED, this);

    auto session = service->get(id);
    if (session) {
        lv_obj_t* title = lv_label_create(list_host_);
        lv_label_set_text(title, session.value().name.c_str());
        lv_obj_set_style_text_color(title, lv_semantic(SemanticColor::TextPrimary), 0);
        lv_obj_set_style_text_font(title, &ibm_plex_mono_24, 0);
    }

    auto obs = service->observations(id);
    if (!obs) {
        set_status("Could not read observations.");
        return;
    }

    if (obs.value().empty()) {
        lv_obj_t* empty = lv_label_create(list_host_);
        lv_label_set_text(empty, "No observations recorded yet.");
        lv_obj_set_style_text_color(empty, lv_semantic(SemanticColor::TextSecondary), 0);
        lv_obj_set_style_text_font(empty, &ibm_plex_mono_18, 0);
        return;
    }

    for (const auto& o : obs.value()) {
        lv_obj_t* row = lv_obj_create(list_host_);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(row, lv_semantic(SemanticColor::SurfaceRaised), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, tokens::RadiusSm, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, tokens::SpaceMd, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        char line[128];
        std::snprintf(line, sizeof(line), "%s  -  %s  -  %d dBm", observation_type_name(o.type),
                      o.source.c_str(), o.signal_strength);
        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, line);
        lv_obj_set_style_text_color(lbl, lv_semantic(SemanticColor::TextPrimary), 0);
        lv_obj_set_style_text_font(lbl, &ibm_plex_mono_16, 0);
    }

    char status[64];
    std::snprintf(status, sizeof(status), "%d observations", static_cast<int>(obs.value().size()));
    set_status(status);
}

void SessionsScreen::on_back_clicked(lv_event_t* event)
{
    auto* self = static_cast<SessionsScreen*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->build_list();
    }
}

void SessionsScreen::on_new_clicked(lv_event_t* event)
{
    auto* self    = static_cast<SessionsScreen*>(lv_event_get_user_data(event));
    auto* service = application::session_service();
    if (self == nullptr || service == nullptr) {
        return;
    }

    auto existing = service->list();
    const int index = existing ? static_cast<int>(existing.value().size()) + 1 : 1;
    char name[48];
    std::snprintf(name, sizeof(name), "Session %02d", index);

    auto created = service->create(name);
    if (created) {
        char msg[96];
        std::snprintf(msg, sizeof(msg), "Created %s", created.value().name.c_str());
        self->set_status(msg);
    } else {
        self->set_status(created.error().message.c_str());
    }
    self->build_list();
}

void SessionsScreen::on_row_action(lv_event_t* event)
{
    auto* ctx     = static_cast<RowCtx*>(lv_event_get_user_data(event));
    auto* service = application::session_service();
    if (ctx == nullptr || ctx->self == nullptr || service == nullptr) {
        return;
    }
    SessionsScreen* self = ctx->self;

    switch (ctx->action) {
        case RowAction::View: {
            self->build_detail(ctx->id);
            return;
        }
        case RowAction::AddObservation: {
            Status st = service->record_observation(ctx->id, ObservationType::Generic, "manual", -50,
                                                    {{"note", "manual marker"}});
            self->set_status(st ? "Observation recorded." : st.error().message.c_str());
            break;
        }
        case RowAction::Export: {
            auto r = service->export_jsonl(ctx->id);
            if (r) {
                std::string msg = "Exported to " + r.value();
                self->set_status(msg.c_str());
            } else {
                self->set_status(r.error().message.c_str());
            }
            break;
        }
        case RowAction::ToggleState: {
            Status st = ctx->status == SessionStatus::Ended ? service->reopen(ctx->id)
                                                            : service->end(ctx->id);
            self->set_status(st ? "Session updated." : st.error().message.c_str());
            break;
        }
        case RowAction::Delete: {
            Status st = service->remove(ctx->id);
            self->set_status(st ? "Session deleted." : st.error().message.c_str());
            break;
        }
    }
    self->build_list();
}

}  // namespace spectra5::ui

#include "ui/components/metric_card.hpp"

#include "ui/design_system/lv_color.hpp"

namespace spectra5::ui {

using tokens::SemanticColor;

MetricCard::MetricCard(lv_obj_t* parent, const std::string& title)
{
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 196, 120);
    lv_obj_set_style_bg_color(root_, lv_semantic(SemanticColor::SurfaceRaised), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(root_, tokens::RadiusMd, 0);
    lv_obj_set_style_border_width(root_, 1, 0);
    lv_obj_set_style_border_color(root_, lv_semantic(SemanticColor::Border), 0);
    lv_obj_set_style_pad_all(root_, tokens::SpaceMd, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    accent_ = lv_obj_create(root_);
    lv_obj_set_size(accent_, 36, 4);
    lv_obj_align(accent_, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_radius(accent_, 2, 0);
    lv_obj_set_style_border_width(accent_, 0, 0);
    lv_obj_set_style_bg_color(accent_, lv_semantic(SemanticColor::Accent), 0);
    lv_obj_set_style_bg_opa(accent_, LV_OPA_COVER, 0);

    lv_obj_t* title_lbl = lv_label_create(root_);
    lv_label_set_text(title_lbl, title.c_str());
    lv_obj_set_style_text_color(title_lbl, lv_semantic(SemanticColor::TextSecondary), 0);
    lv_obj_set_style_text_font(title_lbl, &ibm_plex_mono_16, 0);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_LEFT, 0, tokens::SpaceMd);

    value_ = lv_label_create(root_);
    lv_label_set_text(value_, "--");
    lv_obj_set_style_text_color(value_, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(value_, &ibm_plex_mono_32, 0);
    lv_obj_align(value_, LV_ALIGN_LEFT_MID, 0, 6);

    caption_ = lv_label_create(root_);
    lv_label_set_text(caption_, "");
    lv_obj_set_style_text_color(caption_, lv_semantic(SemanticColor::TextSecondary), 0);
    lv_obj_set_style_text_font(caption_, &ibm_plex_mono_14, 0);
    lv_obj_align(caption_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

void MetricCard::set_value(const std::string& value)
{
    lv_label_set_text(value_, value.c_str());
}

void MetricCard::set_caption(const std::string& caption)
{
    lv_label_set_text(caption_, caption.c_str());
}

void MetricCard::set_accent(SemanticColor color)
{
    lv_obj_set_style_bg_color(accent_, lv_semantic(color), 0);
}

}  // namespace spectra5::ui

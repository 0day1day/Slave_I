#pragma once

#include <lvgl.h>

#include "ui/design_system/theme.hpp"
#include "ui/design_system/tokens.hpp"
#include "ui/fonts/ibm_plex_mono.hpp"  // tactical type; lv_font_montserrat_* -> ibm_plex_mono_*

// Puente entre el theme puro y LVGL. Solo se usa desde la capa de UI.
namespace spectra5::ui {

inline lv_color_t lv_semantic(tokens::SemanticColor c)
{
    return lv_color_hex(active_theme().color(c));
}

// Brand-paint an LVGL input widget (textarea / dropdown). The default LVGL theme
// renders these a cool grey that clashes with the warm Field Armor palette, so we
// override the surface + border + text on creation. For dropdowns this also paints
// the popup list via lv_dropdown_get_list().
inline void brand_input(lv_obj_t* w)
{
    using tokens::SemanticColor;
    lv_obj_set_style_bg_color(w, lv_semantic(SemanticColor::Surface), 0);
    lv_obj_set_style_bg_opa(w, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(w, lv_semantic(SemanticColor::Border), 0);
    lv_obj_set_style_border_width(w, 1, 0);
    lv_obj_set_style_text_color(w, lv_semantic(SemanticColor::TextPrimary), 0);
}

}  // namespace spectra5::ui

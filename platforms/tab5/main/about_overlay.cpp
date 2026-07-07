#include <cstdint>

#include <lvgl.h>

#include "core/version.hpp"
#include "ui/design_system/lv_color.hpp"

// Embedded Mandalorian crest PNG (EMBED_FILES "assets/crest.png").
extern const uint8_t crest_png_start[] asm("_binary_crest_png_start");
extern const uint8_t crest_png_end[] asm("_binary_crest_png_end");

namespace {

using spectra5::ui::lv_semantic;
using SemanticColor = spectra5::ui::tokens::SemanticColor;
namespace tokens = spectra5::ui::tokens;

lv_image_dsc_t g_crest{};

void close_about(lv_event_t* e)
{
    lv_obj_t* modal = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
    if (modal != nullptr) {
        lv_obj_del_async(modal);
    }
}

lv_obj_t* text(lv_obj_t* parent, const char* s, SemanticColor color, const lv_font_t* font)
{
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, lv_pct(100));
    lv_label_set_text(l, s);
    lv_obj_set_style_text_color(l, lv_semantic(color), 0);
    lv_obj_set_style_text_font(l, font, 0);
    return l;
}

}  // namespace

// Weak hook called from the navigation rail's "About" button (app layer).
extern "C" void spectra5_show_about()
{
    lv_obj_t* modal = lv_obj_create(lv_layer_top());
    lv_obj_set_size(modal, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(modal, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(modal, LV_OPA_80, 0);
    lv_obj_set_style_border_width(modal, 0, 0);
    lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(modal, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(modal, &close_about, LV_EVENT_CLICKED, modal);

    lv_obj_t* panel = lv_obj_create(modal);
    lv_obj_set_size(panel, 720, 560);
    lv_obj_set_style_bg_color(panel, lv_semantic(SemanticColor::Surface), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, tokens::RadiusMd, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_semantic(SemanticColor::Accent), 0);
    lv_obj_set_style_pad_all(panel, tokens::SpaceXl, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    // Crest watermark, faded, centered behind the text.
    g_crest.header.magic = LV_IMAGE_HEADER_MAGIC;
    g_crest.header.cf    = LV_COLOR_FORMAT_RAW;
    g_crest.header.w     = 360;
    g_crest.header.h     = 360;
    g_crest.data         = crest_png_start;
    g_crest.data_size    = static_cast<uint32_t>(crest_png_end - crest_png_start);
    lv_obj_t* crest      = lv_image_create(panel);
    lv_image_set_src(crest, &g_crest);
    lv_obj_align(crest, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_image_opa(crest, LV_OPA_20, 0);  // ghosted background

    // Foreground content column (transparent, scrolls over the crest).
    lv_obj_t* col = lv_obj_create(panel);
    lv_obj_set_size(col, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_style_pad_row(col, tokens::SpaceSm, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(col, LV_DIR_VER);

    lv_obj_t* title = text(col, "SLAVE I", SemanticColor::TextPrimary, &ibm_plex_mono_32);
    lv_obj_set_style_text_letter_space(title, 6, 0);
    text(col, "Offensive wireless toolkit for the M5Stack Tab5", SemanticColor::Accent,
         &ibm_plex_mono_18);
    text(col, spectra5::kVersionLabel, SemanticColor::TextSecondary, &ibm_plex_mono_16);

    text(col, LV_SYMBOL_HOME "   idiotsandwich.club", SemanticColor::Accent,
         &ibm_plex_mono_24);
    text(col, LV_SYMBOL_BELL "   hello@idiotsandwich.club", SemanticColor::Accent,
         &ibm_plex_mono_24);

    // Short, punchy, English-with-slang. No churro.
    text(col,
         "Real talk: don't be a caraculo or a scriptkiddie. Run it on your own watch -- "
         "this thing was built to learn, and that's it.",
         SemanticColor::TextPrimary, &ibm_plex_mono_18);

    text(col, "This is the Way.", SemanticColor::Accent, &ibm_plex_mono_18);
    text(col,
         "Built solo -- with solder, coffee and stubbornness, on the shoulders of open "
         "source. If it taught you something, it did its job. Stay curious. Fly casual.",
         SemanticColor::TextPrimary, &ibm_plex_mono_16);

    text(col, "tap anywhere to close", SemanticColor::TextSecondary, &ibm_plex_mono_16);
}

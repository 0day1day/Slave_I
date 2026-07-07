#include "boot_splash.hpp"

#include <cstdint>

#include <lvgl.h>

#include "hal/hal.h"
#include "ui/design_system/lv_color.hpp"

// Embedded helmet PNG (EMBED_FILES "assets/helmet.png").
extern const uint8_t helmet_png_start[] asm("_binary_helmet_png_start");
extern const uint8_t helmet_png_end[] asm("_binary_helmet_png_end");

namespace spectra5::platform {

using spectra5::ui::lv_semantic;
using SemanticColor = spectra5::ui::tokens::SemanticColor;

namespace {

// Random Mandalorian / Boba Fett one-liners for the loading line.
const char* const kQuotes[] = {
    "He's no good to me jammed.",
    "Acquiring targets...",
    "Bounty acquired: your Wi-Fi.",
    "This is the Way... to the handshake.",
    "No disintegrations. Just deauths.",
    "Tracking probe requests...",
    "Put the packets in the cargo hold.",
    "Calibrating reticle...",
    "Dank farrik, where's the SD card?",
    "I'm just a simple radio in the spectrum.",
    "As you wish, operator.",
    "Sleep tight... you're the AP now.",
};

// Epic one-liners for the one-time C6 provisioning screen.
const char* const kProvQuotes[] = {
    "He's no good to me jammed. Arming him.",
    "Loading the beskar payload...",
    "Priming the disintegrator...",
    "Bounty hardware coming online.",
    "This is the Way.",
    "Teaching the C6 some manners.",
    "Melting down a Jawa for spare cycles.",
    "Sharpening the reticle...",
};

lv_image_dsc_t g_helmet{};

void splash_anim_done(lv_anim_t* a)
{
    lv_obj_t* overlay = static_cast<lv_obj_t*>(a->user_data);
    if (overlay != nullptr) {
        lv_obj_del_async(overlay);  // reveal the app underneath
    }
}

void set_bar_value(void* bar, int32_t v)
{
    lv_bar_set_value(static_cast<lv_obj_t*>(bar), v, LV_ANIM_OFF);
}

}  // namespace

void show_boot_splash()
{
    // Subtle startup chime as the helmet appears (persistent audio task -> no
    // task-delete abort under SPIRAM XIP; kept quiet on purpose).
    if (auto* hal = GetHAL()) {
        hal->setSpeakerVolume(28);
        hal->playStartupSfx();
    }

    lv_obj_t* ov = lv_obj_create(lv_layer_top());
    lv_obj_set_size(ov, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(ov, lv_semantic(SemanticColor::Background), 0);
    lv_obj_set_style_bg_opa(ov, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ov, 0, 0);
    lv_obj_set_style_pad_all(ov, 0, 0);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);

    // Helmet logo (decoded from the embedded PNG via LODEPNG).
    g_helmet.header.magic = LV_IMAGE_HEADER_MAGIC;
    g_helmet.header.cf    = LV_COLOR_FORMAT_RAW;
    g_helmet.header.w     = 173;
    g_helmet.header.h     = 260;
    g_helmet.data         = helmet_png_start;
    g_helmet.data_size    = static_cast<uint32_t>(helmet_png_end - helmet_png_start);
    lv_obj_t* img         = lv_image_create(ov);
    lv_image_set_src(img, &g_helmet);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, -110);

    lv_obj_t* title = lv_label_create(ov);
    lv_label_set_text(title, "SLAVE I");
    lv_obj_set_style_text_color(title, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(title, &ibm_plex_mono_32, 0);
    lv_obj_set_style_text_letter_space(title, 8, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 40);

    lv_obj_t* sub = lv_label_create(ov);
    lv_label_set_text(sub, "OFFENSIVE WIRELESS TOOLKIT");
    lv_obj_set_style_text_color(sub, lv_semantic(SemanticColor::Accent), 0);
    lv_obj_set_style_text_font(sub, &ibm_plex_mono_16, 0);
    lv_obj_set_style_text_letter_space(sub, 4, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 86);

    // Random quote.
    lv_obj_t* quote = lv_label_create(ov);
    const unsigned idx = static_cast<unsigned>(lv_tick_get()) % (sizeof(kQuotes) / sizeof(kQuotes[0]));
    lv_label_set_text(quote, kQuotes[idx]);
    lv_obj_set_style_text_color(quote, lv_semantic(SemanticColor::TextSecondary), 0);
    lv_obj_set_style_text_font(quote, &ibm_plex_mono_16, 0);
    lv_obj_align(quote, LV_ALIGN_CENTER, 0, 130);

    // Loading bar.
    lv_obj_t* bar = lv_bar_create(ov);
    lv_obj_set_size(bar, 360, 6);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, 168);
    lv_obj_set_style_bg_color(bar, lv_semantic(SemanticColor::Border), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_semantic(SemanticColor::Accent), LV_PART_INDICATOR);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);

    // Tactical corner brackets.
    for (int i = 0; i < 4; ++i) {
        lv_obj_t* c = lv_obj_create(ov);
        lv_obj_set_size(c, 28, 28);
        lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(c, 0, 0);
        lv_obj_set_style_border_color(c, lv_semantic(SemanticColor::Accent), 0);
        lv_obj_set_style_border_width(c, 2, 0);
        const bool top  = (i < 2);
        const bool left = (i % 2 == 0);
        lv_border_side_t side = static_cast<lv_border_side_t>(
            (top ? LV_BORDER_SIDE_TOP : LV_BORDER_SIDE_BOTTOM) |
            (left ? LV_BORDER_SIDE_LEFT : LV_BORDER_SIDE_RIGHT));
        lv_obj_set_style_border_side(c, side, 0);
        lv_obj_align(c, top ? (left ? LV_ALIGN_TOP_LEFT : LV_ALIGN_TOP_RIGHT)
                            : (left ? LV_ALIGN_BOTTOM_LEFT : LV_ALIGN_BOTTOM_RIGHT),
                     left ? 24 : -24, top ? 24 : -24);
    }

    // Fill the bar over ~2.6s, then remove the whole splash.
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, bar);
    lv_anim_set_exec_cb(&a, set_bar_value);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_time(&a, 5500);  // ~5.5s so the helmet sits long enough to read the quote
    lv_anim_set_user_data(&a, ov);
    lv_anim_set_ready_cb(&a, splash_anim_done);
    lv_anim_start(&a);
}

void show_provisioning_overlay()
{
    auto* hal = GetHAL();
    if (hal != nullptr) {
        hal->lvglLock();
    }

    lv_obj_t* ov = lv_obj_create(lv_layer_top());
    lv_obj_set_size(ov, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(ov, lv_semantic(SemanticColor::Background), 0);
    lv_obj_set_style_bg_opa(ov, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ov, 0, 0);
    lv_obj_set_style_pad_all(ov, 0, 0);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);

    g_helmet.header.magic = LV_IMAGE_HEADER_MAGIC;
    g_helmet.header.cf    = LV_COLOR_FORMAT_RAW;
    g_helmet.header.w     = 173;
    g_helmet.header.h     = 260;
    g_helmet.data         = helmet_png_start;
    g_helmet.data_size    = static_cast<uint32_t>(helmet_png_end - helmet_png_start);
    lv_obj_t* img         = lv_image_create(ov);
    lv_image_set_src(img, &g_helmet);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, -120);

    lv_obj_t* title = lv_label_create(ov);
    lv_label_set_text(title, "ARMING RADIO");
    lv_obj_set_style_text_color(title, lv_semantic(SemanticColor::Accent), 0);
    lv_obj_set_style_text_font(title, &ibm_plex_mono_32, 0);
    lv_obj_set_style_text_letter_space(title, 6, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 40);

    lv_obj_t* quote = lv_label_create(ov);
    const unsigned idx =
        static_cast<unsigned>(lv_tick_get()) % (sizeof(kProvQuotes) / sizeof(kProvQuotes[0]));
    lv_label_set_text(quote, kProvQuotes[idx]);
    lv_obj_set_style_text_color(quote, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(quote, &ibm_plex_mono_16, 0);
    lv_obj_align(quote, LV_ALIGN_CENTER, 0, 84);

    lv_obj_t* sub = lv_label_create(ov);
    lv_label_set_text(sub, "First boot: arming the C6 radio -- do not unplug (~20s).");
    lv_obj_set_style_text_color(sub, lv_semantic(SemanticColor::TextSecondary), 0);
    lv_obj_set_style_text_font(sub, &ibm_plex_mono_16, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 116);

    // Spinner: driven by the LVGL task, so it keeps turning while app_main blocks
    // on the OTA -- clear "working" feedback instead of a frozen screen.
    lv_obj_t* sp = lv_spinner_create(ov);
    lv_spinner_set_anim_params(sp, 1000, 60);
    lv_obj_set_size(sp, 58, 58);
    lv_obj_align(sp, LV_ALIGN_CENTER, 0, 170);
    lv_obj_set_style_arc_color(sp, lv_semantic(SemanticColor::Border), LV_PART_MAIN);
    lv_obj_set_style_arc_color(sp, lv_semantic(SemanticColor::Accent), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(sp, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(sp, 6, LV_PART_INDICATOR);

    if (hal != nullptr) {
        hal->lvglUnlock();
    }
}

void show_provisioning_done()
{
    auto* hal = GetHAL();
    if (hal != nullptr) {
        hal->lvglLock();
    }

    lv_obj_t* ov = lv_obj_create(lv_layer_top());
    lv_obj_set_size(ov, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(ov, lv_semantic(SemanticColor::Background), 0);
    lv_obj_set_style_bg_opa(ov, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ov, 0, 0);
    lv_obj_set_style_pad_all(ov, 0, 0);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);

    g_helmet.header.magic = LV_IMAGE_HEADER_MAGIC;
    g_helmet.header.cf    = LV_COLOR_FORMAT_RAW;
    g_helmet.header.w     = 173;
    g_helmet.header.h     = 260;
    g_helmet.data         = helmet_png_start;
    g_helmet.data_size    = static_cast<uint32_t>(helmet_png_end - helmet_png_start);
    lv_obj_t* img         = lv_image_create(ov);
    lv_image_set_src(img, &g_helmet);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, -100);

    lv_obj_t* title = lv_label_create(ov);
    lv_label_set_text(title, LV_SYMBOL_OK "  RADIO ARMED");
    lv_obj_set_style_text_color(title, lv_semantic(SemanticColor::Success), 0);
    lv_obj_set_style_text_font(title, &ibm_plex_mono_32, 0);
    lv_obj_set_style_text_letter_space(title, 6, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 60);

    lv_obj_t* sub = lv_label_create(ov);
    lv_label_set_text(sub, "Rebooting -- this is the Way.");
    lv_obj_set_style_text_color(sub, lv_semantic(SemanticColor::TextSecondary), 0);
    lv_obj_set_style_text_font(sub, &ibm_plex_mono_16, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 104);

    if (hal != nullptr) {
        hal->lvglUnlock();
    }
}

}  // namespace spectra5::platform

// Weak hook so the cross-platform AppShell can raise the splash at the very start
// of its build (covering the UI as it renders, instead of flashing it first).
extern "C" void spectra5_show_boot_splash()
{
    spectra5::platform::show_boot_splash();
}

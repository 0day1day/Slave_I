// Pixel-perfect screen capture to microSD. Triggered by a hidden long-press on the
// status-bar wordmark (see status_bar.cpp) so it adds no visible UI to the frame.
// The snapshot is taken BEFORE any toast is shown, so the saved image stays clean.
//
// Output: /sd/spectra5/shots/shot_YYYYMMDD_HHMMSS.bmp (24-bit BMP, no encoder needed).

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/stat.h>

#include <lvgl.h>

#include "core/diagnostics/log.hpp"
#include "ui/design_system/lv_color.hpp"

namespace {

constexpr const char* kTag = "shot";

using spectra5::ui::lv_semantic;
using SemanticColor = spectra5::ui::tokens::SemanticColor;
namespace tokens      = spectra5::ui::tokens;

void put_u32(uint8_t* p, uint32_t v) { std::memcpy(p, &v, 4); }
void put_u16(uint8_t* p, uint16_t v) { std::memcpy(p, &v, 2); }

// 24-bit bottom-up BMP. LVGL RGB888 is stored B,G,R in memory -- the same order BMP
// wants -- so rows copy straight across (verified on device).
bool write_bmp24(const char* path, const uint8_t* px, int w, int h, int stride)
{
    FILE* f = std::fopen(path, "wb");
    if (f == nullptr) {
        return false;
    }
    const int row      = w * 3;
    const int pad      = (4 - (row % 4)) % 4;
    const uint32_t img = static_cast<uint32_t>((row + pad) * h);

    uint8_t hdr[54] = {0};
    hdr[0] = 'B';
    hdr[1] = 'M';
    put_u32(hdr + 2, 54 + img);
    put_u32(hdr + 10, 54);                          // pixel data offset
    put_u32(hdr + 14, 40);                          // DIB header size
    put_u32(hdr + 18, static_cast<uint32_t>(w));
    put_u32(hdr + 22, static_cast<uint32_t>(h));    // positive height => bottom-up
    put_u16(hdr + 26, 1);                           // planes
    put_u16(hdr + 28, 24);                          // bits per pixel
    put_u32(hdr + 34, img);
    std::fwrite(hdr, 1, sizeof(hdr), f);

    const uint8_t zero[3] = {0, 0, 0};
    for (int y = h - 1; y >= 0; --y) {              // LVGL buffer is top-down
        std::fwrite(px + static_cast<std::size_t>(y) * stride, 1, static_cast<std::size_t>(row), f);
        if (pad != 0) {
            std::fwrite(zero, 1, static_cast<std::size_t>(pad), f);
        }
    }
    std::fclose(f);
    return true;
}

void toast_expire(lv_timer_t* t)
{
    auto* o = static_cast<lv_obj_t*>(lv_timer_get_user_data(t));
    if (o != nullptr) {
        lv_obj_del_async(o);
    }
    lv_timer_del(t);
}

void toast(const char* msg, bool ok)
{
    lv_obj_t* t = lv_label_create(lv_layer_top());
    lv_label_set_text(t, msg);
    lv_obj_set_style_text_color(t, lv_semantic(SemanticColor::TextPrimary), 0);
    lv_obj_set_style_text_font(t, &ibm_plex_mono_16, 0);
    lv_obj_set_style_bg_color(t, lv_semantic(ok ? SemanticColor::Success : SemanticColor::Danger), 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(t, tokens::RadiusSm, 0);
    lv_obj_set_style_pad_all(t, tokens::SpaceSm, 0);
    lv_obj_align(t, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_timer_create(toast_expire, 1400, t);
}

}  // namespace

// Hook called from the status bar (declared extern "C" there; desktop provides a stub).
extern "C" void spectra5_screenshot()
{
    lv_obj_t* scr = lv_screen_active();
    if (scr == nullptr) {
        return;
    }

    lv_draw_buf_t* snap = lv_snapshot_take(scr, LV_COLOR_FORMAT_RGB888);
    if (snap == nullptr) {
        spectra5::log::tagError(kTag, "lv_snapshot_take failed (out of memory?)");
        toast("screenshot failed", false);
        return;
    }

    ::mkdir("/sd/spectra5", 0777);
    ::mkdir("/sd/spectra5/shots", 0777);

    char name[48];
    char path[96];
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
    localtime_r(&t, &tmv);
    std::strftime(name, sizeof(name), "shot_%Y%m%d_%H%M%S.bmp", &tmv);
    std::snprintf(path, sizeof(path), "/sd/spectra5/shots/%s", name);

    const bool ok = write_bmp24(path, static_cast<const uint8_t*>(snap->data),
                                static_cast<int>(snap->header.w), static_cast<int>(snap->header.h),
                                static_cast<int>(snap->header.stride));
    lv_draw_buf_destroy(snap);

    if (ok) {
        spectra5::log::tagInfo(kTag, "saved {}", path);
        toast(name, true);
    } else {
        spectra5::log::tagError(kTag, "SD write failed for {}", path);
        toast("screenshot: SD error", false);
    }
}

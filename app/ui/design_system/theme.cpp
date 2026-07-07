#include "ui/design_system/theme.hpp"

namespace spectra5::ui {

using tokens::SemanticColor;

namespace {

// "Field Armor" -- weathered Mandalorian / Boba Fett: olive-black, amber visor,
// cream stencil, rust wear marks. The default theme (Design A from the mockups).
std::uint32_t dark(SemanticColor c)
{
    switch (c) {
        case SemanticColor::Background:      return 0x14120D;  // deepest olive-black
        case SemanticColor::Surface:         return 0x1E1A12;  // card / panel
        case SemanticColor::SurfaceRaised:   return 0x2A261C;  // raised card / button
        case SemanticColor::Border:          return 0x3A3326;  // olive edge / wear line
        case SemanticColor::TextPrimary:     return 0xE8E2D2;  // cream stencil
        case SemanticColor::TextSecondary:   return 0xA39B84;  // weathered tan
        case SemanticColor::Accent:          return 0xD4A017;  // amber visor (Boba gold)
        case SemanticColor::Success:         return 0x9BB24A;  // olive green
        case SemanticColor::Warning:         return 0xCE8E1E;  // burnt amber
        case SemanticColor::Danger:          return 0xB8472F;  // rust red (used and lethal)
        case SemanticColor::Info:            return 0x6E7A45;  // muted olive
        case SemanticColor::RadioWifi:       return 0xD4A017;  // amber
        case SemanticColor::RadioBle:        return 0x9BB24A;  // olive green
        case SemanticColor::RadioIeee802154: return 0x6E7A45;  // olive
        case SemanticColor::RadioRf:         return 0xB8472F;  // rust
    }
    return 0xFFFFFF;
}

std::uint32_t high_contrast(SemanticColor c)
{
    switch (c) {
        case SemanticColor::Background:      return 0x000000;
        case SemanticColor::Surface:         return 0x0A0A0A;
        case SemanticColor::SurfaceRaised:   return 0x171717;
        case SemanticColor::Border:          return 0x4D4D4D;
        case SemanticColor::TextPrimary:     return 0xFFFFFF;
        case SemanticColor::TextSecondary:   return 0xC8C8C8;
        case SemanticColor::Accent:          return 0x60A5FA;
        case SemanticColor::Success:         return 0x4ADE80;
        case SemanticColor::Warning:         return 0xFBBF24;
        case SemanticColor::Danger:          return 0xF87171;
        case SemanticColor::Info:            return 0x7DD3FC;
        case SemanticColor::RadioWifi:       return 0x60A5FA;
        case SemanticColor::RadioBle:        return 0xA78BFA;
        case SemanticColor::RadioIeee802154: return 0x2DD4BF;
        case SemanticColor::RadioRf:         return 0xFB923C;
    }
    return 0xFFFFFF;
}

}  // namespace

std::uint32_t Theme::color(SemanticColor semantic) const
{
    switch (id_) {
        case ThemeId::HighContrast: return high_contrast(semantic);
        case ThemeId::Dark:
        default:                    return dark(semantic);
    }
}

namespace {
Theme g_active_theme{ThemeId::Dark};
}

const Theme& active_theme()
{
    return g_active_theme;
}

void set_active_theme(ThemeId id)
{
    g_active_theme = Theme{id};
}

}  // namespace spectra5::ui

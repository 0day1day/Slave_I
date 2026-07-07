#pragma once

#include <cstdint>

// Tokens de diseño puros: sin dependencias de LVGL/SDL/ESP-IDF.
// Los módulos no deben usar valores hexadecimales directos; resuelven color
// a través de SemanticColor + Theme (ver theme.hpp).
namespace spectra5::ui::tokens {

inline constexpr int SpaceXs = 4;
inline constexpr int SpaceSm = 8;
inline constexpr int SpaceMd = 16;
inline constexpr int SpaceLg = 24;
inline constexpr int SpaceXl = 32;

inline constexpr int RadiusSm = 6;
inline constexpr int RadiusMd = 12;
inline constexpr int RadiusLg = 18;

inline constexpr int TouchTarget    = 48;
inline constexpr int TouchSpacingMin = 8;

// Dimensiones del layout principal (sección 11.2 del PRD).
inline constexpr int StatusBarHeight     = 48;
inline constexpr int NavigationRailWidth = 100;  // wider rail for roomier icons/labels
inline constexpr int TaskBarHeight       = 44;

inline constexpr int ScreenWidth  = 1280;
inline constexpr int ScreenHeight = 720;

enum class SemanticColor : std::uint8_t {
    Background,
    Surface,
    SurfaceRaised,
    Border,
    TextPrimary,
    TextSecondary,
    Accent,
    Success,
    Warning,
    Danger,
    Info,
    RadioWifi,
    RadioBle,
    RadioIeee802154,
    RadioRf,
};

}  // namespace spectra5::ui::tokens

#pragma once

#include <cstdint>

#include "ui/design_system/tokens.hpp"

// Theme puro: mapea SemanticColor -> color RGB de 24 bits (0xRRGGBB).
// No depende de LVGL para ser testable de forma aislada; la capa de
// componentes convierte el valor a lv_color_t.
namespace spectra5::ui {

enum class ThemeId : std::uint8_t {
    Dark,
    HighContrast,
};

class Theme {
public:
    explicit Theme(ThemeId id = ThemeId::Dark) : id_(id) {}

    ThemeId id() const { return id_; }

    // Devuelve el color en formato 0xRRGGBB.
    std::uint32_t color(tokens::SemanticColor semantic) const;

private:
    ThemeId id_;
};

// Tema activo global (UI-thread). Sustituible para pruebas/escenarios.
const Theme& active_theme();
void set_active_theme(ThemeId id);

}  // namespace spectra5::ui

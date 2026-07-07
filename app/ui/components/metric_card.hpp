#pragma once

#include <string>

#include <lvgl.h>

#include "ui/design_system/tokens.hpp"

// Tarjeta de métrica: título, valor grande y estado coloreado opcional.
// Permite actualizar el valor sin reconstruir el widget (sección 13, criterio 4).
namespace spectra5::ui {

class MetricCard {
public:
    // Crea la tarjeta dentro de `parent`. No toma propiedad del parent.
    MetricCard(lv_obj_t* parent, const std::string& title);

    lv_obj_t* root() const { return root_; }

    void set_value(const std::string& value);
    void set_caption(const std::string& caption);
    void set_accent(tokens::SemanticColor color);

private:
    lv_obj_t* root_    = nullptr;
    lv_obj_t* value_   = nullptr;
    lv_obj_t* caption_ = nullptr;
    lv_obj_t* accent_  = nullptr;
};

}  // namespace spectra5::ui

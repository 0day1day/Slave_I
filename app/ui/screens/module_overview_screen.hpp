#pragma once

#include <lvgl.h>

#include "services/system/system_service.hpp"
#include "ui/navigation/navigation_model.hpp"

namespace spectra5::ui {

// Pantalla informativa de módulo: cabecera, estado de disponibilidad,
// hardware requerido, descripción y fase de desarrollo. No es un placeholder
// genérico; cumple la sección 12 (módulos con explicación y requisitos).
class ModuleOverviewScreen {
public:
    ModuleOverviewScreen(lv_obj_t* parent, const NavItem& item, const services::CapabilitySet& caps);

private:
    lv_obj_t* root_ = nullptr;
};

}  // namespace spectra5::ui

#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <lvgl.h>

#include "ui/navigation/navigation_model.hpp"

namespace spectra5::ui {

// Left navigation rail (88 px). Builds one icon+label button per NavItem and
// delegates selection to the NavigationModel. Highlights the active route and
// dims modules whose capability is unavailable.
class NavigationRail {
public:
    NavigationRail(lv_obj_t* parent, NavigationModel& model, const services::CapabilitySet& caps);

    lv_obj_t* root() const { return root_; }

    void set_active(Route route);

private:
    struct Button {
        Route route;
        lv_obj_t* obj;
        lv_obj_t* icon;
        lv_obj_t* label;
        bool enabled;
    };

    using FlyItem = std::pair<std::string, std::function<void()>>;

    static void on_click(lv_event_t* event);
    void show_flyout(lv_obj_t* anchor, const std::vector<FlyItem>& items);

    NavigationModel& model_;
    lv_obj_t* root_ = nullptr;
    std::vector<Button> buttons_;
};

// Glyph icon (LVGL/FontAwesome symbol) for each route.
const char* route_icon(Route route);

}  // namespace spectra5::ui

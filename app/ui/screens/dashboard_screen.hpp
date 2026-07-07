#pragma once

#include <memory>
#include <vector>

#include <lvgl.h>

#include "services/system/system_service.hpp"
#include "ui/components/metric_card.hpp"

namespace spectra5::ui {

// Pantalla Dashboard (sección 13). Construye una rejilla de MetricCards y
// las actualiza sin reconstruir la pantalla.
class DashboardScreen {
public:
    explicit DashboardScreen(lv_obj_t* parent);

    void update(const services::SystemMetrics& metrics);

private:
    enum Card {
        CardC6,
        CardStorage,
        CardPower,
        CardTemp,
        CardMemory,
        CardRadio,
        CardDevices,
        CardTasks,
        // --- tool-activity KPIs (this session) ---
        CardDeauths,
        CardBeacons,
        CardBleSpam,
        CardZigbee,
        CardHandshakes,
        CardCreds,
        CardCount,
    };

    lv_obj_t* root_ = nullptr;
    std::vector<std::unique_ptr<MetricCard>> cards_;
};

}  // namespace spectra5::ui

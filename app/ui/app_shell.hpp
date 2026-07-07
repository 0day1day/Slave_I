#pragma once

#include <memory>

#include <lvgl.h>

#include "services/system/system_service.hpp"
#include "ui/components/navigation_rail.hpp"
#include "ui/components/status_bar.hpp"
#include "ui/design_system/theme.hpp"
#include "ui/navigation/navigation_model.hpp"
#include "ui/screens/ble_screen.hpp"
#include "ui/screens/dashboard_screen.hpp"
#include "ui/screens/files_screen.hpp"
#include "ui/screens/module_overview_screen.hpp"
#include "ui/screens/nearby_screen.hpp"
#include "ui/screens/sessions_screen.hpp"
#include "ui/screens/settings_screen.hpp"
#include "ui/screens/wifi_screen.hpp"
#include "ui/screens/workflows_screen.hpp"
#include "ui/screens/zigbee_screen.hpp"

namespace spectra5::ui {

// Application shell: StatusBar + NavigationRail + content host + task bar.
// Observes the NavigationModel and rebuilds only the content host on route
// change. Metric refresh never rebuilds the screen. A theme change rebuilds
// the whole shell so the new palette is applied everywhere.
class AppShell {
public:
    AppShell(lv_obj_t* parent, const services::CapabilitySet& caps);

    void refresh(const services::SystemMetrics& metrics);
    void set_clock(const char* hhmm);

private:
    void build();
    void rebuild();
    void show_route(Route route);

    lv_obj_t* parent_ = nullptr;
    services::CapabilitySet caps_;
    NavigationModel model_;

    lv_obj_t* root_         = nullptr;
    lv_obj_t* content_host_ = nullptr;
    lv_obj_t* task_bar_     = nullptr;

    std::unique_ptr<StatusBar> status_bar_;
    std::unique_ptr<NavigationRail> nav_rail_;
    std::unique_ptr<DashboardScreen> dashboard_;
    std::unique_ptr<ModuleOverviewScreen> module_screen_;
    std::unique_ptr<SettingsScreen> settings_;
    std::unique_ptr<SessionsScreen> sessions_;
    std::unique_ptr<FilesScreen> files_;
    std::unique_ptr<WorkflowsScreen> workflows_;
    std::unique_ptr<WifiScreen> wifi_;
    std::unique_ptr<BleScreen> ble_;
    std::unique_ptr<ZigbeeScreen> zigbee_;
    std::unique_ptr<NearbyScreen> nearby_;
};

}  // namespace spectra5::ui

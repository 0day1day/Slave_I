#include "ui/navigation/navigation_model.hpp"

namespace spectra5::ui {

using services::Capability;

NavigationModel::NavigationModel()
{
    items_ = {
        {Route::Dashboard, "Dashboard", Capability::None},
        {Route::Nearby, "Nearby", Capability::None},
        {Route::Wifi, "Wi-Fi", Capability::None},
        {Route::Bluetooth, "Bluetooth", Capability::None},
        {Route::Ieee802154, "802.15.4", Capability::None},  // live: C6 energy scan
        {Route::External, "External", Capability::None},    // hub: RF / NFC / IR (Grove modules)
        {Route::Sessions, "Sessions", Capability::None},
        {Route::Files, "Files", Capability::StorageSd},
        {Route::Settings, "Settings", Capability::None},  // Diagnostics folded in here
    };
}

bool NavigationModel::select(Route route)
{
    if (route == current_) {
        return false;
    }
    current_ = route;
    notify();
    return true;
}

bool NavigationModel::is_available(Route route, const services::CapabilitySet& caps) const
{
    const NavItem* item = find(route);
    if (item == nullptr) {
        return false;
    }
    return caps.has(item->required_capability);
}

void NavigationModel::add_observer(Observer observer)
{
    if (observer) {
        observers_.push_back(std::move(observer));
    }
}

const NavItem* NavigationModel::find(Route route) const
{
    for (const auto& item : items_) {
        if (item.route == route) {
            return &item;
        }
    }
    return nullptr;
}

void NavigationModel::notify()
{
    for (auto& observer : observers_) {
        observer(current_);
    }
}

const char* route_id(Route route)
{
    switch (route) {
        case Route::Dashboard:   return "dashboard";
        case Route::Nearby:      return "nearby";
        case Route::Wifi:        return "wifi";
        case Route::Bluetooth:   return "bluetooth";
        case Route::Ieee802154:  return "ieee802154";
        case Route::External:    return "external";
        case Route::Rf:          return "rf";
        case Route::NfcRfid:     return "nfc-rfid";
        case Route::Infrared:    return "infrared";
        case Route::Ports:       return "ports";
        case Route::Sessions:    return "sessions";
        case Route::Workflows:   return "workflows";
        case Route::Files:       return "files";
        case Route::Diagnostics: return "diagnostics";
        case Route::Settings:    return "settings";
    }
    return "unknown";
}

}  // namespace spectra5::ui

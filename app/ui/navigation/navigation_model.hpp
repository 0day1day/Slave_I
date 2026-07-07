#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "services/capabilities/capability.hpp"

// Modelo de navegación puro (sin LVGL). Gestiona el menú principal,
// la ruta activa y notifica cambios mediante observadores.
namespace spectra5::ui {

enum class Route : std::uint8_t {
    Dashboard,
    Nearby,
    Wifi,
    Bluetooth,
    Ieee802154,
    External,  // hub for external-hardware modules (RF / NFC / IR)
    Rf,
    NfcRfid,
    Infrared,
    Ports,
    Sessions,
    Workflows,
    Files,
    Diagnostics,
    Settings,
};

struct NavItem {
    Route route;
    std::string label;
    // Capacidad requerida para que el módulo esté operativo. None => siempre.
    services::Capability required_capability = services::Capability::None;
};

class NavigationModel {
public:
    using Observer = std::function<void(Route)>;

    NavigationModel();

    const std::vector<NavItem>& items() const { return items_; }

    Route current() const { return current_; }

    // Devuelve true si la ruta cambió (notifica a los observadores).
    bool select(Route route);

    // ¿Está el módulo disponible según el conjunto de capacidades dado?
    bool is_available(Route route, const services::CapabilitySet& caps) const;

    void add_observer(Observer observer);

    const NavItem* find(Route route) const;

private:
    void notify();

    std::vector<NavItem> items_;
    std::vector<Observer> observers_;
    Route current_ = Route::Dashboard;
};

const char* route_id(Route route);

}  // namespace spectra5::ui

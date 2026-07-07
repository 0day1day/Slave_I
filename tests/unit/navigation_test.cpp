#include <cassert>

#include "ui/navigation/navigation_model.hpp"

using namespace spectra5::ui;
using spectra5::services::Capability;
using spectra5::services::CapabilitySet;

int main()
{
    NavigationModel model;

    // Estado inicial.
    assert(model.current() == Route::Dashboard);
    assert(model.items().size() == 9);  // RF/NFC/IR -> External flyout; Diagnostics -> Settings; Workflows + Ports removed

    // Observador recibe el cambio de ruta.
    Route observed = Route::Dashboard;
    int calls      = 0;
    model.add_observer([&](Route r) {
        observed = r;
        ++calls;
    });

    assert(model.select(Route::Wifi) == true);
    assert(model.current() == Route::Wifi);
    assert(observed == Route::Wifi);
    assert(calls == 1);

    // Seleccionar la misma ruta no notifica.
    assert(model.select(Route::Wifi) == false);
    assert(calls == 1);

    // Disponibilidad según capacidades.
    CapabilitySet none;
    assert(model.is_available(Route::Dashboard, none) == true);   // sin requisito
    assert(model.is_available(Route::Wifi, none) == true);        // ahora siempre disponible
    assert(model.is_available(Route::Files, none) == false);      // requiere StorageSd

    CapabilitySet with_sd{Capability::StorageSd};
    assert(model.is_available(Route::Files, with_sd) == true);

    // find() coherente.
    const NavItem* item = model.find(Route::Settings);
    assert(item != nullptr);
    assert(item->required_capability == Capability::None);
    assert(model.find(static_cast<Route>(250)) == nullptr);

    return 0;
}

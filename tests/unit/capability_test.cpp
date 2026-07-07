#include <cassert>
#include <cstring>

#include "services/capabilities/capability.hpp"

using namespace spectra5::services;

int main()
{
    CapabilitySet set;

    // None siempre verdadero.
    assert(set.has(Capability::None) == true);
    assert(set.has(Capability::WifiScan) == false);

    set.add(Capability::WifiScan);
    set.add(Capability::BleScan);
    assert(set.has(Capability::WifiScan) == true);
    assert(set.has(Capability::BleScan) == true);
    assert(set.has(Capability::Nfc) == false);

    set.remove(Capability::WifiScan);
    assert(set.has(Capability::WifiScan) == false);
    assert(set.has(Capability::BleScan) == true);

    // Inicialización por lista.
    CapabilitySet init{Capability::Nfc, Capability::Gps};
    assert(init.has(Capability::Nfc) == true);
    assert(init.has(Capability::Gps) == true);
    assert(init.has(Capability::Audio) == false);

    init.clear();
    assert(init.has(Capability::Nfc) == false);

    // Etiquetas no vacías.
    assert(std::strlen(capability_label(Capability::WifiScan)) > 0);

    return 0;
}

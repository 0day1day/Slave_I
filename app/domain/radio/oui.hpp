#pragma once

#include "domain/radio/offensive.hpp"

namespace spectra5::domain {

// Best-effort MAC -> vendor name from the OUI (first 3 bytes). Returns "(random)"
// for locally-administered (randomized) MACs and "" when the OUI is unknown. Backed
// by a small curated table of common consumer vendors -- not the full IEEE registry.
const char* oui_vendor(const MacAddr& mac);

}  // namespace spectra5::domain

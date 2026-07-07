#pragma once

namespace spectra5::core {

// RTTI-free, stable per-type key. ESP-IDF builds with -fno-rtti, so typeid is
// unavailable; the address of a per-type static marker gives a unique, stable
// identifier usable as a map key for the event/command buses.
using TypeKey = const void*;

template <class T>
inline TypeKey type_key()
{
    static const char marker = 0;
    return &marker;
}

}  // namespace spectra5::core

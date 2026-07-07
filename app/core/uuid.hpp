#pragma once

#include <cstdint>
#include <mutex>
#include <random>
#include <string>

namespace spectra5 {

// Generates short, collision-resistant identifiers suitable for filesystem
// directory names, e.g. "sess-1a2b3c4d5e6f7a8b". Thread-safe.
inline std::string generate_id(const std::string& prefix)
{
    static std::mt19937_64 rng([] {
        std::random_device rd;
        return (static_cast<uint64_t>(rd()) << 32) ^ static_cast<uint64_t>(rd());
    }());
    static std::mutex mtx;
    static uint64_t counter = 0;

    uint64_t a;
    uint64_t mix;
    {
        std::lock_guard<std::mutex> lock(mtx);
        a   = rng();
        mix = a ^ (0x9E3779B97F4A7C15ull * (++counter));
    }

    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(prefix.size() + 17);
    if (!prefix.empty()) {
        out += prefix;
        out += '-';
    }
    for (int i = 60; i >= 0; i -= 4) {
        out += hex[(mix >> i) & 0xF];
    }
    return out;
}

}  // namespace spectra5

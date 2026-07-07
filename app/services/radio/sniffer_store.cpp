#include "services/radio/sniffer_store.hpp"

namespace spectra5::services {

SnifferStore& SnifferStore::instance()
{
    static SnifferStore s;
    return s;
}

void SnifferStore::set(const std::array<uint32_t, 5>& counts)
{
    std::lock_guard<std::mutex> lock(mutex_);
    counts_ = counts;
    ++revision_;
}

std::array<uint32_t, 5> SnifferStore::snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return counts_;
}

uint32_t SnifferStore::total() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t t = 0;
    for (uint32_t c : counts_) {
        t += c;
    }
    return t;
}

uint32_t SnifferStore::revision() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return revision_;
}

void SnifferStore::reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    counts_ = {};
    ++revision_;
}

}  // namespace spectra5::services

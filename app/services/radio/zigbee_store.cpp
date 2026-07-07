#include "services/radio/zigbee_store.hpp"

namespace spectra5::services {

ZigbeeStore& ZigbeeStore::instance()
{
    static ZigbeeStore s;
    return s;
}

void ZigbeeStore::set(const std::array<int8_t, 16>& powers)
{
    std::lock_guard<std::mutex> lock(mutex_);
    powers_ = powers;
    ++revision_;
}

std::array<int8_t, 16> ZigbeeStore::snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return powers_;
}

uint32_t ZigbeeStore::revision() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return revision_;
}

bool ZigbeeStore::has_result() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return revision_ > 0;
}

}  // namespace spectra5::services

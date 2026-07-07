#include "services/radio/deauth_detector.hpp"

namespace spectra5::services {

DeauthDetector& DeauthDetector::instance()
{
    static DeauthDetector d;
    return d;
}

void DeauthDetector::report(const std::string& src)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ++count_;
    ++revision_;
    last_src_ = src;
}

uint32_t DeauthDetector::count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
}

std::string DeauthDetector::last_src() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return last_src_;
}

uint32_t DeauthDetector::revision() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return revision_;
}

void DeauthDetector::reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    count_    = 0;
    last_src_ = {};
    ++revision_;
}

}  // namespace spectra5::services

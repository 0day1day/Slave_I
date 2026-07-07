#include "services/storage/sd_logger.hpp"

#include <sys/stat.h>

#include <cstdio>

namespace spectra5::services {

namespace {
constexpr const char* kRoot = "/sd/spectra5";

void make_dir(const char* path)
{
    ::mkdir(path, 0777);  // ignore EEXIST
}
}  // namespace

SdLogger& SdLogger::instance()
{
    static SdLogger logger;
    return logger;
}

void SdLogger::ensure_dirs()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (dirs_ready_) {
        return;
    }
    make_dir(kRoot);
    make_dir("/sd/spectra5/wifi");
    make_dir("/sd/spectra5/ble");
    make_dir("/sd/spectra5/portals");
    dirs_ready_ = true;
}

void SdLogger::enqueue(const std::string& relpath, std::string line)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.size() < 512) {  // cap to bound memory if the SD is missing
        queue_.emplace_back(relpath, std::move(line));
    }
}

void SdLogger::flush()
{
    std::vector<std::pair<std::string, std::string>> work;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return;
        }
        work.swap(queue_);
    }
    for (const auto& [rel, line] : work) {
        const std::string full = std::string(kRoot) + "/" + rel;
        FILE* f                = std::fopen(full.c_str(), "a");
        if (f == nullptr) {
            continue;  // SD missing/unwritable -- drop (already dequeued)
        }
        std::fputs(line.c_str(), f);
        std::fputc('\n', f);
        std::fclose(f);
    }
}

std::size_t SdLogger::pending() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

}  // namespace spectra5::services

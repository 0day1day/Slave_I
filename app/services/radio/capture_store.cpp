#include "services/radio/capture_store.hpp"

namespace spectra5::services {

CaptureStore& CaptureStore::instance()
{
    static CaptureStore store;
    return store;
}

void CaptureStore::set_target(const std::string& essid)
{
    std::lock_guard<std::mutex> lock(mutex_);
    essid_       = essid;
    pmkid_count_ = 0;
    eapol_count_ = 0;
    last_msg_    = 0;
    ++revision_;
}

std::string CaptureStore::essid() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return essid_;
}

void CaptureStore::on_pmkid()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ++pmkid_count_;
    ++revision_;
}

void CaptureStore::on_eapol(uint8_t msg)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ++eapol_count_;
    last_msg_ = msg;
    ++revision_;
}

void CaptureStore::queue_line(const std::string& line)
{
    std::lock_guard<std::mutex> lock(mutex_);
    pending_lines_.push_back(line);
}

std::vector<std::string> CaptureStore::drain_lines()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> out;
    out.swap(pending_lines_);
    return out;
}

int CaptureStore::pmkid_count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return pmkid_count_;
}

int CaptureStore::eapol_count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return eapol_count_;
}

uint8_t CaptureStore::last_msg() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return last_msg_;
}

uint32_t CaptureStore::revision() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return revision_;
}

}  // namespace spectra5::services

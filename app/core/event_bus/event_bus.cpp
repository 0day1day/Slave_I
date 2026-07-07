#include "core/event_bus/event_bus.hpp"

#include <utility>

namespace spectra5::core {

void EventBus::unsubscribe(SubscriptionId id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto type_it = id_to_type_.find(id);
    if (type_it == id_to_type_.end()) {
        return;
    }
    auto bucket_it = handlers_.find(type_it->second);
    if (bucket_it != handlers_.end()) {
        auto& vec = bucket_it->second;
        for (auto it = vec.begin(); it != vec.end(); ++it) {
            if (it->id == id) {
                vec.erase(it);
                break;
            }
        }
        if (vec.empty()) {
            handlers_.erase(bucket_it);
        }
    }
    id_to_type_.erase(type_it);
}

std::size_t EventBus::subscriber_count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return id_to_type_.size();
}

bool EventBus::post(std::function<void()> fn)
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (queue_.size() >= capacity_) {
        ++dropped_;
        return false;
    }
    queue_.push_back(std::move(fn));
    return true;
}

std::size_t EventBus::drain(std::size_t max_items)
{
    std::size_t processed = 0;
    while (processed < max_items) {
        std::function<void()> fn;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (queue_.empty()) {
                break;
            }
            fn = std::move(queue_.front());
            queue_.pop_front();
        }
        fn();
        ++processed;
    }
    return processed;
}

void EventBus::set_capacity(std::size_t capacity)
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    capacity_ = capacity == 0 ? 1 : capacity;
}

std::size_t EventBus::queued() const
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return queue_.size();
}

std::uint64_t EventBus::dropped() const
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return dropped_;
}

}  // namespace spectra5::core

#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "core/type_id.hpp"

namespace spectra5::core {

using SubscriptionId = uint64_t;

// Typed, thread-safe publish/subscribe bus.
//
// - publish<E>() dispatches synchronously to all subscribers of event type E
//   (handlers are copied under the lock and invoked outside it, so a handler may
//   safely subscribe/unsubscribe/publish during delivery).
// - The deferred queue (post/drain) decouples producers from a single consumer
//   thread (e.g. the LVGL/UI thread): producers post() closures, the UI thread
//   calls drain(). The queue is bounded; once full, posts are dropped and
//   counted so backpressure is observable.
class EventBus {
public:
    EventBus() = default;
    EventBus(const EventBus&)            = delete;
    EventBus& operator=(const EventBus&) = delete;

    template <class E>
    SubscriptionId subscribe(std::function<void(const E&)> handler)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const SubscriptionId id = next_id_++;
        const TypeKey key       = type_key<E>();
        handlers_[key].push_back(Entry{id, [h = std::move(handler)](const void* ev) {
                                            h(*static_cast<const E*>(ev));
                                        }});
        id_to_type_.emplace(id, key);
        return id;
    }

    template <class E>
    void publish(const E& event)
    {
        std::vector<std::function<void(const void*)>> snapshot;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = handlers_.find(type_key<E>());
            if (it == handlers_.end()) {
                return;
            }
            snapshot.reserve(it->second.size());
            for (const auto& entry : it->second) {
                snapshot.push_back(entry.fn);
            }
        }
        for (auto& fn : snapshot) {
            fn(&event);
        }
    }

    void unsubscribe(SubscriptionId id);
    std::size_t subscriber_count() const;

    // Deferred (queued) delivery.
    bool post(std::function<void()> fn);
    std::size_t drain(std::size_t max_items = SIZE_MAX);
    void set_capacity(std::size_t capacity);
    std::size_t queued() const;
    std::uint64_t dropped() const;

private:
    struct Entry {
        SubscriptionId id;
        std::function<void(const void*)> fn;
    };

    mutable std::mutex mutex_;
    std::unordered_map<TypeKey, std::vector<Entry>> handlers_;
    std::unordered_map<SubscriptionId, TypeKey> id_to_type_;
    SubscriptionId next_id_ = 1;

    mutable std::mutex queue_mutex_;
    std::deque<std::function<void()>> queue_;
    std::size_t capacity_   = 256;
    std::uint64_t dropped_  = 0;
};

}  // namespace spectra5::core

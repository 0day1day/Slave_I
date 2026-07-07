#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

#include "core/result.hpp"
#include "core/type_id.hpp"

namespace spectra5::core {

// Typed command dispatcher. Each command type has exactly one handler, which
// performs the action and returns a Status. UI widgets dispatch commands and
// never touch services/drivers directly (see PRD 8.4).
class CommandBus {
public:
    CommandBus() = default;
    CommandBus(const CommandBus&)            = delete;
    CommandBus& operator=(const CommandBus&) = delete;

    template <class C>
    void register_handler(std::function<Status(const C&)> handler)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_[type_key<C>()] = [h = std::move(handler)](const void* cmd) {
            return h(*static_cast<const C*>(cmd));
        };
    }

    template <class C>
    void unregister_handler()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.erase(type_key<C>());
    }

    template <class C>
    bool has_handler() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return handlers_.count(type_key<C>()) > 0;
    }

    template <class C>
    Status dispatch(const C& command) const
    {
        std::function<Status(const void*)> handler;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = handlers_.find(type_key<C>());
            if (it == handlers_.end()) {
                return Status::fail(ErrorCode::Unavailable, "no handler registered for command");
            }
            handler = it->second;
        }
        return handler(&command);
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<TypeKey, std::function<Status(const void*)>> handlers_;
};

}  // namespace spectra5::core

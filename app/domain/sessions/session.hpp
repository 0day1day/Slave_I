#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/clock.hpp"

namespace spectra5::domain {

using SessionId = std::string;
using Tag       = std::string;

enum class SessionStatus {
    Draft,
    Active,
    Paused,
    Ended,
    Archived,
};

inline const char* session_status_name(SessionStatus s)
{
    switch (s) {
        case SessionStatus::Draft:    return "draft";
        case SessionStatus::Active:   return "active";
        case SessionStatus::Paused:   return "paused";
        case SessionStatus::Ended:    return "ended";
        case SessionStatus::Archived: return "archived";
    }
    return "draft";
}

inline SessionStatus session_status_from(const std::string& s)
{
    if (s == "active") return SessionStatus::Active;
    if (s == "paused") return SessionStatus::Paused;
    if (s == "ended") return SessionStatus::Ended;
    if (s == "archived") return SessionStatus::Archived;
    return SessionStatus::Draft;
}

// A research session groups all information of one investigation (PRD §22).
struct Session {
    SessionId id;
    std::string name;
    std::string description;
    Timestamp created_at = 0;
    Timestamp started_at = 0;
    std::optional<Timestamp> ended_at;
    SessionStatus status = SessionStatus::Draft;
    std::vector<Tag> tags;
    int observation_count = 0;
};

}  // namespace spectra5::domain

#pragma once

#include <string>
#include <vector>

#include "core/result.hpp"
#include "domain/observations/observation.hpp"
#include "domain/sessions/session.hpp"

namespace spectra5::domain {

// Abstract persistence boundary for sessions and their observations. The domain
// and application layers depend only on this interface; concrete stores live in
// the services/platform layers (e.g. filesystem on desktop / SD card on Tab5).
class ISessionRepository {
public:
    virtual ~ISessionRepository() = default;

    virtual Status create(const Session& session)                                 = 0;
    virtual Status update(const Session& session)                                 = 0;
    virtual Result<Session> get(const SessionId& id)                              = 0;
    virtual Result<std::vector<Session>> list()                                   = 0;
    virtual Status remove(const SessionId& id)                                    = 0;

    virtual Status append_observation(const Observation& observation)             = 0;
    virtual Result<std::vector<Observation>> observations(const SessionId& id)    = 0;

    // Writes a combined JSONL export and returns the absolute export path.
    virtual Result<std::string> export_jsonl(const SessionId& id)                 = 0;
};

}  // namespace spectra5::domain

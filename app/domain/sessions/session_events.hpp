#pragma once

#include "domain/observations/observation.hpp"
#include "domain/sessions/session.hpp"

namespace spectra5::domain {

// Typed events published on the EventBus by the session use cases (PRD §8.3).
struct SessionCreated {
    Session session;
};

struct SessionEnded {
    SessionId id;
};

struct SessionReopened {
    SessionId id;
};

struct ObservationRecorded {
    Observation observation;
};

struct SessionExported {
    SessionId id;
    std::string path;
};

}  // namespace spectra5::domain

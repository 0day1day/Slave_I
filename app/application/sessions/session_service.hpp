#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/clock.hpp"
#include "core/event_bus/event_bus.hpp"
#include "core/result.hpp"
#include "domain/observations/observation.hpp"
#include "domain/sessions/session.hpp"
#include "domain/sessions/session_repository.hpp"

namespace spectra5::application {

// Application use cases for sessions (PRD §22). Pure logic: depends only on the
// abstract repository, a clock and (optionally) the event bus. No UI, no HAL.
class SessionService {
public:
    SessionService(domain::ISessionRepository& repository, IClock& clock,
                   core::EventBus* bus = nullptr);

    Result<domain::Session> create(const std::string& name, const std::string& description = "",
                                   const std::vector<domain::Tag>& tags = {});

    Result<std::vector<domain::Session>> list();
    Result<domain::Session> get(const domain::SessionId& id);

    Status rename(const domain::SessionId& id, const std::string& name);
    Status pause(const domain::SessionId& id);
    Status resume(const domain::SessionId& id);
    Status end(const domain::SessionId& id);
    Status reopen(const domain::SessionId& id);
    Status archive(const domain::SessionId& id);
    Status remove(const domain::SessionId& id);

    Status record_observation(const domain::SessionId& session_id, domain::ObservationType type,
                              const std::string& source, int signal_strength,
                              const domain::MetadataMap& metadata = {});

    Result<std::vector<domain::Observation>> observations(const domain::SessionId& id);
    Result<std::string> export_jsonl(const domain::SessionId& id);

private:
    Result<domain::Session> mutate(const domain::SessionId& id,
                                   const std::function<void(domain::Session&)>& fn);

    domain::ISessionRepository& repo_;
    IClock& clock_;
    core::EventBus* bus_;
};

// Injectable singleton (same pattern as the HAL / system service). The platform
// owns the repository, clock and bus and keeps them alive for the service.
SessionService* session_service();
void inject_session_service(std::unique_ptr<SessionService> service);
void destroy_session_service();
bool has_session_service();

}  // namespace spectra5::application

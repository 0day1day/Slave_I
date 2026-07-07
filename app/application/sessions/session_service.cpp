#include "application/sessions/session_service.hpp"

#include "core/uuid.hpp"
#include "domain/sessions/session_events.hpp"

namespace spectra5::application {

using namespace spectra5::domain;

SessionService::SessionService(ISessionRepository& repository, IClock& clock, core::EventBus* bus)
    : repo_(repository), clock_(clock), bus_(bus)
{
}

Result<Session> SessionService::create(const std::string& name, const std::string& description,
                                       const std::vector<Tag>& tags)
{
    if (name.empty()) {
        return Result<Session>::fail(ErrorCode::InvalidArgument, "session name is required");
    }

    Session session;
    session.id          = generate_id("sess");
    session.name        = name;
    session.description = description;
    session.created_at  = clock_.now_ms();
    session.started_at  = session.created_at;
    session.status      = SessionStatus::Active;
    session.tags        = tags;

    Status st = repo_.create(session);
    if (!st) {
        return Result<Session>::fail(st.error().code, st.error().message);
    }
    if (bus_ != nullptr) {
        bus_->publish(SessionCreated{session});
    }
    return Result<Session>::ok(session);
}

Result<std::vector<Session>> SessionService::list()
{
    return repo_.list();
}

Result<Session> SessionService::get(const SessionId& id)
{
    return repo_.get(id);
}

Result<Session> SessionService::mutate(const SessionId& id,
                                       const std::function<void(Session&)>& fn)
{
    auto current = repo_.get(id);
    if (!current) {
        return current;
    }
    Session updated = current.value();
    fn(updated);
    Status st = repo_.update(updated);
    if (!st) {
        return Result<Session>::fail(st.error().code, st.error().message);
    }
    return Result<Session>::ok(updated);
}

Status SessionService::rename(const SessionId& id, const std::string& name)
{
    if (name.empty()) {
        return Status::fail(ErrorCode::InvalidArgument, "session name is required");
    }
    auto r = mutate(id, [&](Session& s) { s.name = name; });
    return r ? Status::ok() : Status(r.error());
}

Status SessionService::pause(const SessionId& id)
{
    auto r = mutate(id, [](Session& s) { s.status = SessionStatus::Paused; });
    return r ? Status::ok() : Status(r.error());
}

Status SessionService::resume(const SessionId& id)
{
    auto r = mutate(id, [](Session& s) { s.status = SessionStatus::Active; });
    return r ? Status::ok() : Status(r.error());
}

Status SessionService::end(const SessionId& id)
{
    auto r = mutate(id, [&](Session& s) {
        s.status   = SessionStatus::Ended;
        s.ended_at = clock_.now_ms();
    });
    if (!r) {
        return Status(r.error());
    }
    if (bus_ != nullptr) {
        bus_->publish(SessionEnded{id});
    }
    return Status::ok();
}

Status SessionService::reopen(const SessionId& id)
{
    auto r = mutate(id, [](Session& s) {
        s.status = SessionStatus::Active;
        s.ended_at.reset();
    });
    if (!r) {
        return Status(r.error());
    }
    if (bus_ != nullptr) {
        bus_->publish(SessionReopened{id});
    }
    return Status::ok();
}

Status SessionService::archive(const SessionId& id)
{
    auto r = mutate(id, [](Session& s) { s.status = SessionStatus::Archived; });
    return r ? Status::ok() : Status(r.error());
}

Status SessionService::remove(const SessionId& id)
{
    return repo_.remove(id);
}

Status SessionService::record_observation(const SessionId& session_id, ObservationType type,
                                          const std::string& source, int signal_strength,
                                          const MetadataMap& metadata)
{
    Observation obs;
    obs.id              = generate_id("obs");
    obs.session_id      = session_id;
    obs.type            = type;
    obs.timestamp       = clock_.now_ms();
    obs.source          = source;
    obs.signal_strength = signal_strength;
    obs.metadata        = metadata;

    Status st = repo_.append_observation(obs);
    if (!st) {
        return st;
    }
    if (bus_ != nullptr) {
        bus_->publish(ObservationRecorded{obs});
    }
    return Status::ok();
}

Result<std::vector<Observation>> SessionService::observations(const SessionId& id)
{
    return repo_.observations(id);
}

Result<std::string> SessionService::export_jsonl(const SessionId& id)
{
    auto r = repo_.export_jsonl(id);
    if (r && bus_ != nullptr) {
        bus_->publish(SessionExported{id, r.value()});
    }
    return r;
}

namespace {
std::unique_ptr<SessionService>& service_slot()
{
    static std::unique_ptr<SessionService> instance;
    return instance;
}
}  // namespace

SessionService* session_service()
{
    return service_slot().get();
}

void inject_session_service(std::unique_ptr<SessionService> service)
{
    service_slot() = std::move(service);
}

void destroy_session_service()
{
    service_slot().reset();
}

bool has_session_service()
{
    return static_cast<bool>(service_slot());
}

}  // namespace spectra5::application

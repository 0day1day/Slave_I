#include <unistd.h>

#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

#include "application/sessions/session_service.hpp"
#include "core/clock.hpp"
#include "services/storage/filesystem_session_store.hpp"

using namespace spectra5;
using namespace spectra5::domain;

namespace {
int count_lines(const std::string& path)
{
    std::ifstream in(path);
    if (!in) {
        return -1;
    }
    int n = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) {
            ++n;
        }
    }
    return n;
}
}  // namespace

int main()
{
    const std::string base = "/tmp/spectra5_test_" + std::to_string(::getpid());

    ManualClock clock(1000);
    services::FilesystemSessionStore store(base);
    application::SessionService svc(store, clock);

    // Create.
    auto created = svc.create("Field Survey", "desc", {"tag-a"});
    assert(created);
    const SessionId id = created.value().id;
    assert(!id.empty());
    assert(created.value().status == SessionStatus::Active);

    // Empty name is rejected.
    assert(!svc.create(""));

    // List has exactly one.
    auto listed = svc.list();
    assert(listed);
    assert(listed.value().size() == 1);

    // Record observations.
    clock.advance(50);
    assert(svc.record_observation(id, ObservationType::WifiAp, "ap-1", -42, {{"ssid", "demo"}}));
    clock.advance(50);
    assert(svc.record_observation(id, ObservationType::BleDevice, "dev-1", -70));

    auto obs = svc.observations(id);
    assert(obs);
    assert(obs.value().size() == 2);

    // Count is persisted in session.json.
    auto fetched = svc.get(id);
    assert(fetched);
    assert(fetched.value().observation_count == 2);

    // End -> Ended with timestamp.
    assert(svc.end(id));
    fetched = svc.get(id);
    assert(fetched.value().status == SessionStatus::Ended);
    assert(fetched.value().ended_at.has_value());

    // Reopen -> Active again.
    assert(svc.reopen(id));
    fetched = svc.get(id);
    assert(fetched.value().status == SessionStatus::Active);
    assert(!fetched.value().ended_at.has_value());

    // Export JSONL: header + 2 observations = 3 lines.
    auto exported = svc.export_jsonl(id);
    assert(exported);
    assert(count_lines(exported.value()) == 3);

    // Persistence across a fresh store instance (reopen the repository).
    {
        services::FilesystemSessionStore store2(base);
        application::SessionService svc2(store2, clock);
        auto again = svc2.list();
        assert(again);
        assert(again.value().size() == 1);
        assert(again.value()[0].observation_count == 2);
    }

    // Delete leaves the store empty.
    assert(svc.remove(id));
    auto after = svc.list();
    assert(after);
    assert(after.value().empty());

    return 0;
}

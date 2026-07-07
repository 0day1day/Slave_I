#include <unistd.h>

#include <cassert>
#include <chrono>
#include <string>
#include <thread>

#include "application/sessions/session_service.hpp"
#include "application/workflows/workflow_engine.hpp"
#include "core/clock.hpp"
#include "core/scheduler/task_manager.hpp"
#include "services/storage/filesystem_session_store.hpp"

using namespace spectra5;
using namespace spectra5::domain;

namespace {
void wait_until_idle(application::WorkflowEngine& engine)
{
    for (int i = 0; i < 200 && engine.running(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}
}  // namespace

int main()
{
    const std::string base = "/tmp/spectra5_wf_" + std::to_string(::getpid());

    SystemClock clock;
    services::FilesystemSessionStore store(base);
    application::SessionService sessions(store, clock);
    core::TaskManager tasks;
    application::WorkflowEngine engine(sessions, tasks);

    // Validation rejects unknown actions and missing params.
    Workflow bad;
    bad.steps.push_back({"does.not.exist", {}});
    assert(!engine.validate(bad));

    Workflow bad2;
    bad2.steps.push_back({"observation.record", {{"type", "wifi"}}});  // missing source
    assert(!engine.validate(bad2));

    assert(engine.validate(application::WorkflowEngine::example()));

    // Run the example workflow to completion.
    auto start = engine.start(application::WorkflowEngine::example());
    assert(start);

    // A second start while running is rejected.
    auto conflict = engine.start(application::WorkflowEngine::example());
    assert(!conflict);

    wait_until_idle(engine);
    auto run = engine.snapshot();
    assert(run.status == WorkflowStatus::Completed);
    assert(run.current_step == run.total_steps);

    // The workflow created exactly one session with two observations.
    auto listed = sessions.list();
    assert(listed);
    assert(listed.value().size() == 1);
    auto obs = sessions.observations(listed.value()[0].id);
    assert(obs);
    assert(obs.value().size() == 2);

    return 0;
}

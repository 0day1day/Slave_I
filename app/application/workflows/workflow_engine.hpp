#pragma once

#include <memory>
#include <mutex>

#include "application/sessions/session_service.hpp"
#include "core/result.hpp"
#include "core/scheduler/task_manager.hpp"
#include "domain/workflows/workflow.hpp"

namespace spectra5::application {

// Executes workflows on a background task with cooperative cancellation,
// progress and logs (PRD §25). Actions map to real session use cases, so a run
// produces real sessions/observations/exports on disk. On desktop this is the
// "simulated execution" path; on Tab5 it runs against the microSD store.
class WorkflowEngine {
public:
    WorkflowEngine(SessionService& sessions, core::TaskManager& tasks);

    // Validates that every step uses a known action with its required params.
    Status validate(const domain::Workflow& workflow) const;

    // Starts a run if none is active. Returns Conflict if already running or
    // InvalidArgument if validation fails.
    Status start(const domain::Workflow& workflow);
    void cancel();

    bool running() const;
    domain::WorkflowRun snapshot() const;

    // Built-in demo workflow: create -> record (wifi/ble) -> wait -> export.
    static domain::Workflow example();

private:
    void execute(const domain::Workflow& workflow, const core::CancellationToken& token);
    void log_line(const std::string& line);

    SessionService& sessions_;
    core::TaskManager& tasks_;

    mutable std::mutex mutex_;
    domain::WorkflowRun run_;
    bool active_ = false;
    core::TaskId task_id_ = 0;
};

// Injectable singleton (platform owns the engine and its dependencies).
WorkflowEngine* workflow_engine();
void inject_workflow_engine(std::unique_ptr<WorkflowEngine> engine);
bool has_workflow_engine();

}  // namespace spectra5::application

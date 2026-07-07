#include "application/workflows/workflow_engine.hpp"

#include <chrono>
#include <cstdlib>
#include <thread>

#include "domain/observations/observation.hpp"

namespace spectra5::application {

using namespace spectra5::domain;

namespace {

int param_int(const WorkflowStep& step, const std::string& key, int fallback)
{
    auto it = step.params.find(key);
    if (it == step.params.end()) {
        return fallback;
    }
    char* end       = nullptr;
    const long value = std::strtol(it->second.c_str(), &end, 10);
    if (end == it->second.c_str()) {
        return fallback;
    }
    return static_cast<int>(value);
}

std::string param_str(const WorkflowStep& step, const std::string& key, const std::string& fallback)
{
    auto it = step.params.find(key);
    return it == step.params.end() ? fallback : it->second;
}

bool has_param(const WorkflowStep& step, const std::string& key)
{
    return step.params.find(key) != step.params.end();
}

}  // namespace

WorkflowEngine::WorkflowEngine(SessionService& sessions, core::TaskManager& tasks)
    : sessions_(sessions), tasks_(tasks)
{
    run_.status = WorkflowStatus::Idle;
}

Status WorkflowEngine::validate(const Workflow& workflow) const
{
    if (workflow.steps.empty()) {
        return Status::fail(ErrorCode::InvalidArgument, "workflow has no steps");
    }
    for (const auto& step : workflow.steps) {
        if (step.action == "session.create") {
            continue;
        } else if (step.action == "observation.record") {
            if (!has_param(step, "type") || !has_param(step, "source")) {
                return Status::fail(ErrorCode::InvalidArgument,
                                    "observation.record requires type and source");
            }
        } else if (step.action == "wait") {
            if (!has_param(step, "ms")) {
                return Status::fail(ErrorCode::InvalidArgument, "wait requires ms");
            }
        } else if (step.action == "session.export") {
            continue;
        } else {
            return Status::fail(ErrorCode::InvalidArgument, "unknown action: " + step.action);
        }
    }
    return Status::ok();
}

Status WorkflowEngine::start(const Workflow& workflow)
{
    Status valid = validate(workflow);
    if (!valid) {
        return valid;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_) {
            return Status::fail(ErrorCode::Conflict, "a workflow is already running");
        }
        active_              = true;
        run_                 = WorkflowRun{};
        run_.status          = WorkflowStatus::Running;
        run_.total_steps     = workflow.steps.size();
        run_.current_step    = 0;
    }

    Workflow copy = workflow;
    task_id_ = tasks_.run("workflow:" + workflow.name,
                          [this, copy](const core::CancellationToken& token) { execute(copy, token); });
    return Status::ok();
}

void WorkflowEngine::cancel()
{
    core::TaskId id;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) {
            return;
        }
        id = task_id_;
    }
    tasks_.cancel(id);
}

bool WorkflowEngine::running() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return active_;
}

WorkflowRun WorkflowEngine::snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return run_;
}

void WorkflowEngine::log_line(const std::string& line)
{
    std::lock_guard<std::mutex> lock(mutex_);
    run_.log.push_back(line);
}

void WorkflowEngine::execute(const Workflow& workflow, const core::CancellationToken& token)
{
    SessionId current_session;

    for (std::size_t i = 0; i < workflow.steps.size(); ++i) {
        if (token.is_cancelled()) {
            std::lock_guard<std::mutex> lock(mutex_);
            run_.status = WorkflowStatus::Cancelled;
            run_.log.push_back("cancelled");
            active_ = false;
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            run_.current_step = i + 1;
        }

        const WorkflowStep& step = workflow.steps[i];
        Status step_status        = Status::ok();

        if (step.action == "session.create") {
            auto created = sessions_.create(param_str(step, "name", "Workflow Session"));
            if (created) {
                current_session = created.value().id;
                log_line("created session " + created.value().name);
            } else {
                step_status = Status(created.error());
            }
        } else if (step.action == "observation.record") {
            if (current_session.empty()) {
                step_status = Status::fail(ErrorCode::InvalidArgument, "no session to record into");
            } else {
                step_status = sessions_.record_observation(
                    current_session, observation_type_from(param_str(step, "type", "generic")),
                    param_str(step, "source", "workflow"), param_int(step, "rssi", -60));
                if (step_status) {
                    log_line("recorded " + param_str(step, "type", "generic") + " from " +
                             param_str(step, "source", "workflow"));
                }
            }
        } else if (step.action == "wait") {
            const int ms = param_int(step, "ms", 0);
            log_line("waiting " + std::to_string(ms) + " ms");
            int elapsed = 0;
            while (elapsed < ms) {
                if (token.is_cancelled()) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                elapsed += 20;
            }
        } else if (step.action == "session.export") {
            if (current_session.empty()) {
                step_status = Status::fail(ErrorCode::InvalidArgument, "no session to export");
            } else {
                auto exported = sessions_.export_jsonl(current_session);
                if (exported) {
                    log_line("exported to " + exported.value());
                } else {
                    step_status = Status(exported.error());
                }
            }
        }

        if (!step_status) {
            std::lock_guard<std::mutex> lock(mutex_);
            run_.status = WorkflowStatus::Failed;
            run_.error  = step_status.error().message;
            run_.log.push_back("error: " + step_status.error().message);
            active_ = false;
            return;
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    run_.status = WorkflowStatus::Completed;
    run_.log.push_back("completed");
    active_ = false;
}

Workflow WorkflowEngine::example()
{
    // Minimal, honest demo: open a session and export it -- no synthetic/fake
    // observations. Real captures come from the actual radio tools. (Workflows is not
    // wired into the nav; this scaffold is kept for possible future use.)
    Workflow wf;
    wf.name = "Session Demo";
    wf.steps.push_back({"session.create", {{"name", "Auto Capture"}}});
    wf.steps.push_back({"wait", {{"ms", "600"}}});
    wf.steps.push_back({"session.export", {}});
    return wf;
}

namespace {
std::unique_ptr<WorkflowEngine>& engine_slot()
{
    static std::unique_ptr<WorkflowEngine> instance;
    return instance;
}
}  // namespace

WorkflowEngine* workflow_engine()
{
    return engine_slot().get();
}

void inject_workflow_engine(std::unique_ptr<WorkflowEngine> engine)
{
    engine_slot() = std::move(engine);
}

bool has_workflow_engine()
{
    return static_cast<bool>(engine_slot());
}

}  // namespace spectra5::application

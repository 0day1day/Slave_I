#pragma once

#include <map>
#include <string>
#include <vector>

namespace spectra5::domain {

// A workflow chains safe, validated operations (PRD §25). Steps reference an
// action by name plus string parameters; the engine maps actions to real use
// cases (session.create, observation.record, wait, session.export).
struct WorkflowStep {
    std::string action;
    std::map<std::string, std::string> params;
};

struct Workflow {
    std::string name;
    std::vector<WorkflowStep> steps;
};

enum class WorkflowStatus {
    Idle,
    Running,
    Completed,
    Failed,
    Cancelled,
};

inline const char* workflow_status_name(WorkflowStatus s)
{
    switch (s) {
        case WorkflowStatus::Idle:      return "idle";
        case WorkflowStatus::Running:   return "running";
        case WorkflowStatus::Completed: return "completed";
        case WorkflowStatus::Failed:    return "failed";
        case WorkflowStatus::Cancelled: return "cancelled";
    }
    return "idle";
}

// Observable execution state, copied atomically for the UI to render.
struct WorkflowRun {
    WorkflowStatus status = WorkflowStatus::Idle;
    std::size_t current_step = 0;
    std::size_t total_steps  = 0;
    std::vector<std::string> log;
    std::string error;
};

}  // namespace spectra5::domain

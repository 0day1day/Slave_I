#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace spectra5::core {

using TaskId = std::uint64_t;

// Cooperative cancellation token shared with a running task. The task must poll
// is_cancelled() and return promptly when set.
class CancellationToken {
public:
    CancellationToken() : flag_(std::make_shared<std::atomic<bool>>(false)) {}
    explicit CancellationToken(std::shared_ptr<std::atomic<bool>> flag) : flag_(std::move(flag)) {}

    bool is_cancelled() const { return flag_ && flag_->load(); }

private:
    std::shared_ptr<std::atomic<bool>> flag_;
};

// Runs named background tasks on std::thread with cooperative cancellation.
// Finished tasks are reaped (joined) lazily and on destruction.
class TaskManager {
public:
    using TaskFn = std::function<void(const CancellationToken&)>;

    TaskManager() = default;
    TaskManager(const TaskManager&)            = delete;
    TaskManager& operator=(const TaskManager&) = delete;
    ~TaskManager();

    TaskId run(std::string name, TaskFn fn);

    void cancel(TaskId id);
    void cancel_all();

    // Blocks until the given task (or all tasks) finished, then reaps it.
    void join(TaskId id);
    void join_all();

    std::size_t running_count() const;
    std::size_t total_count() const;

private:
    struct Task {
        std::string name;
        std::thread thread;
        std::shared_ptr<std::atomic<bool>> cancel_flag;
        std::shared_ptr<std::atomic<bool>> done_flag;
    };

    void reap_finished();

    mutable std::mutex mutex_;
    std::map<TaskId, Task> tasks_;
    TaskId next_id_ = 1;
};

}  // namespace spectra5::core

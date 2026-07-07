#include "core/scheduler/task_manager.hpp"

#include <utility>
#include <vector>

namespace spectra5::core {

TaskManager::~TaskManager()
{
    cancel_all();
    join_all();
}

TaskId TaskManager::run(std::string name, TaskFn fn)
{
    reap_finished();

    auto cancel_flag = std::make_shared<std::atomic<bool>>(false);
    auto done_flag   = std::make_shared<std::atomic<bool>>(false);
    CancellationToken token(cancel_flag);

    std::lock_guard<std::mutex> lock(mutex_);
    const TaskId id = next_id_++;

    Task task;
    task.name        = std::move(name);
    task.cancel_flag = cancel_flag;
    task.done_flag   = done_flag;
    task.thread      = std::thread([fn = std::move(fn), token, done_flag]() {
        fn(token);
        done_flag->store(true);
    });

    tasks_.emplace(id, std::move(task));
    return id;
}

void TaskManager::cancel(TaskId id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tasks_.find(id);
    if (it != tasks_.end() && it->second.cancel_flag) {
        it->second.cancel_flag->store(true);
    }
}

void TaskManager::cancel_all()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, task] : tasks_) {
        if (task.cancel_flag) {
            task.cancel_flag->store(true);
        }
    }
}

void TaskManager::join(TaskId id)
{
    std::thread thread;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tasks_.find(id);
        if (it == tasks_.end()) {
            return;
        }
        thread = std::move(it->second.thread);
        tasks_.erase(it);
    }
    if (thread.joinable()) {
        thread.join();
    }
}

void TaskManager::join_all()
{
    std::vector<std::thread> threads;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [id, task] : tasks_) {
            threads.push_back(std::move(task.thread));
        }
        tasks_.clear();
    }
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

std::size_t TaskManager::running_count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t count = 0;
    for (const auto& [id, task] : tasks_) {
        if (task.done_flag && !task.done_flag->load()) {
            ++count;
        }
    }
    return count;
}

std::size_t TaskManager::total_count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
}

void TaskManager::reap_finished()
{
    std::vector<std::thread> threads;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = tasks_.begin(); it != tasks_.end();) {
            if (it->second.done_flag && it->second.done_flag->load()) {
                threads.push_back(std::move(it->second.thread));
                it = tasks_.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

}  // namespace spectra5::core

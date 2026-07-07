#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>

#include "core/scheduler/task_manager.hpp"

using namespace spectra5::core;

int main()
{
    TaskManager tm;

    // A short task runs to completion and reports its result.
    std::atomic<bool> ran{false};
    TaskId id = tm.run("quick", [&](const CancellationToken&) { ran.store(true); });
    tm.join(id);
    assert(ran.load());

    // A long-running task observes cooperative cancellation.
    std::atomic<int> ticks{0};
    std::atomic<bool> saw_cancel{false};
    TaskId loop = tm.run("loop", [&](const CancellationToken& token) {
        while (!token.is_cancelled()) {
            ticks.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        saw_cancel.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    assert(tm.running_count() >= 1);
    tm.cancel(loop);
    tm.join(loop);
    assert(saw_cancel.load());
    assert(ticks.load() > 0);
    assert(tm.total_count() == 0);

    // cancel_all + destructor cleanup must not hang.
    for (int i = 0; i < 4; ++i) {
        tm.run("bg", [](const CancellationToken& token) {
            while (!token.is_cancelled()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    tm.cancel_all();
    tm.join_all();
    assert(tm.total_count() == 0);

    return 0;
}

#include <cassert>

#include "core/event_bus/event_bus.hpp"

using namespace spectra5::core;

namespace {
struct Ping {
    int value;
};
struct Pong {
    int value;
};
}  // namespace

int main()
{
    EventBus bus;

    int ping_sum = 0;
    int pong_sum = 0;

    auto a = bus.subscribe<Ping>([&](const Ping& p) { ping_sum += p.value; });
    bus.subscribe<Ping>([&](const Ping& p) { ping_sum += p.value * 10; });
    bus.subscribe<Pong>([&](const Pong& p) { pong_sum += p.value; });
    assert(bus.subscriber_count() == 3);

    bus.publish(Ping{2});
    assert(ping_sum == 2 + 20);  // both Ping handlers fired
    assert(pong_sum == 0);       // Pong handler untouched

    bus.publish(Pong{5});
    assert(pong_sum == 5);

    // Unsubscribe removes only the targeted handler.
    bus.unsubscribe(a);
    assert(bus.subscriber_count() == 2);
    ping_sum = 0;
    bus.publish(Ping{1});
    assert(ping_sum == 10);  // only the *10 handler remains

    // Deferred queue: bounded + dropped accounting.
    bus.set_capacity(2);
    int drained = 0;
    assert(bus.post([&] { ++drained; }) == true);
    assert(bus.post([&] { ++drained; }) == true);
    assert(bus.post([&] { ++drained; }) == false);  // full -> dropped
    assert(bus.dropped() == 1);
    assert(bus.queued() == 2);

    const std::size_t processed = bus.drain();
    assert(processed == 2);
    assert(drained == 2);
    assert(bus.queued() == 0);

    return 0;
}

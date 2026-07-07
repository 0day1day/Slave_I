#include <cassert>

#include "core/command_bus/command_bus.hpp"

using namespace spectra5;
using namespace spectra5::core;

namespace {
struct Add {
    int a;
    int b;
};
struct Fail {};
}  // namespace

int main()
{
    CommandBus bus;
    int last_sum = 0;

    assert(bus.has_handler<Add>() == false);

    // Dispatching without a handler fails with Unavailable.
    Status none = bus.dispatch(Add{1, 2});
    assert(!none);
    assert(none.error().code == ErrorCode::Unavailable);

    bus.register_handler<Add>([&](const Add& cmd) {
        last_sum = cmd.a + cmd.b;
        return Status::ok();
    });
    bus.register_handler<Fail>(
        [](const Fail&) { return Status::fail(ErrorCode::Internal, "boom"); });

    assert(bus.has_handler<Add>() == true);

    Status ok = bus.dispatch(Add{3, 4});
    assert(ok);
    assert(last_sum == 7);

    Status bad = bus.dispatch(Fail{});
    assert(!bad);
    assert(bad.error().code == ErrorCode::Internal);

    bus.unregister_handler<Add>();
    assert(bus.has_handler<Add>() == false);

    return 0;
}

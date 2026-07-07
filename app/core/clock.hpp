#pragma once

#include <chrono>
#include <cstdint>

namespace spectra5 {

// Milliseconds since Unix epoch (UTC).
using Timestamp = int64_t;

// Abstract clock so the domain can be tested deterministically (ManualClock)
// while platforms use the real wall clock (SystemClock).
class IClock {
public:
    virtual ~IClock() = default;
    virtual Timestamp now_ms() const = 0;
};

class SystemClock final : public IClock {
public:
    Timestamp now_ms() const override
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }
};

// Deterministic clock for tests; time only advances when asked.
class ManualClock final : public IClock {
public:
    explicit ManualClock(Timestamp start = 0) : now_(start) {}

    Timestamp now_ms() const override { return now_; }
    void advance(Timestamp delta_ms) { now_ += delta_ms; }
    void set(Timestamp value) { now_ = value; }

private:
    Timestamp now_;
};

}  // namespace spectra5

#pragma once

#include <atomic>

namespace amm_shim {

// counts a thread inside a hook for as long as the guard lives, so shutdown can wait until no thread runs hook code
class InFlightCall {
public:
    explicit InFlightCall(std::atomic<int>& count) : mCount(count) { mCount.fetch_add(1); }
    ~InFlightCall() { mCount.fetch_sub(1); }

    InFlightCall(InFlightCall const&)            = delete;
    InFlightCall& operator=(InFlightCall const&) = delete;

private:
    std::atomic<int>& mCount;
};

} // namespace amm_shim

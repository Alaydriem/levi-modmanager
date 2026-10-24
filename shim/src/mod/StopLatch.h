#pragma once

#include <atomic>

namespace amm_shim {

// raised by disable() before the cursor and the HUD are put back, so no frame or tick that is still running can
// publish or apply the manager's cursor or HUD state again afterwards; lowered by enable()
class StopLatch {
public:
    static void raise() { sRaised.store(true, std::memory_order_seq_cst); }

    static void lower() { sRaised.store(false, std::memory_order_seq_cst); }

    static bool raised() { return sRaised.load(std::memory_order_seq_cst); }

private:
    static inline std::atomic<bool> sRaised{false};
};

} // namespace amm_shim

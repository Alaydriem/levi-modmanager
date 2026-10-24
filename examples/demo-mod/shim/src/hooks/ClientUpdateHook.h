#pragma once

#include "demo_ffi.h"

#include <ll/api/memory/Hook.h>
#include <mc/client/game/ClientInstance.h>

namespace demo_shim {

// once per client tick on the game thread: the demo handles the events the manager queued for it
LL_TYPE_INSTANCE_HOOK(
    ClientUpdateHook,
    ll::memory::HookPriority::Normal,
    ClientInstance,
    &ClientInstance::$update,
    bool,
    bool a1
) {
    bool result = origin(a1);
    demo_tick();
    return result;
}

} // namespace demo_shim

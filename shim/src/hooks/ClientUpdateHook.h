#pragma once

#include "amm_host.h"
#include "bridge/RustBridge.h"
#include "hud/HudControl.h"
#include "input/MenuScreen.h"
#include "input/MouseGrab.h"

#include <windows.h>

#include <ll/api/memory/Hook.h>
#include <mc/client/game/ClientInstance.h>

namespace amm_shim {

// once per client tick on the game thread: the manager's tick, the screen on top for the next host frame, then the
// mouse grab and the HUD state the last host frame asked for
LL_TYPE_INSTANCE_HOOK(
    ClientUpdateHook,
    ll::memory::HookPriority::Normal,
    ClientInstance,
    &ClientInstance::$update,
    bool,
    bool a1
) {
    bool result = origin(a1);
    if (!RustBridge::running()) return result;
    amm_host_tick(GetTickCount64());
    MenuScreen::observe(*this);
    MouseGrab::apply(*this);
    HudControl::apply(*this);
    return result;
}

} // namespace amm_shim

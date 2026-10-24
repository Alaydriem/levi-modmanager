#pragma once

#include "input/MouseGrab.h"

#include <ll/api/memory/Hook.h>
#include <mc/client/game/MinecraftGame.h>

namespace amm_shim {

// the game grabs the mouse again on its own while the world shows with no screen; while the host frame keeps the
// cursor free that grab is skipped, so the cursor stays free
LL_TYPE_INSTANCE_HOOK(
    GrabMouseHook,
    ll::memory::HookPriority::Normal,
    MinecraftGame,
    &MinecraftGame::$grabMouse,
    void
) {
    if (MouseGrab::blocksGrab()) return;
    origin();
}

} // namespace amm_shim

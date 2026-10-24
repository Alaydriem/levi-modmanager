#pragma once

#include "amm_host.h"
#include "bridge/RustBridge.h"

#include <ll/api/memory/Hook.h>
#include <mc/client/player/LocalPlayer.h>
#include <mc/deps/core/math/Vec2.h>

namespace amm_shim {

// mouse look reaches the player here; the call is dropped while the host says a mod that holds Input asked for the
// player's own turn to be blocked
LL_TYPE_INSTANCE_HOOK(
    TurnDeltaHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::_applyTurnDelta,
    void,
    Vec2 const& turnOffset
) {
    if (RustBridge::running() && amm_host_blocks_player_turn()) return;
    origin(turnOffset);
}

} // namespace amm_shim

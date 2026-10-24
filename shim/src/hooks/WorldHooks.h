#pragma once

#include "amm_host.h"
#include "bridge/RustBridge.h"

#include <ll/api/event/EventBus.h>
#include <ll/api/event/client/ClientExitLevelEvent.h>
#include <ll/api/event/client/ClientJoinLevelEvent.h>

namespace amm_shim {

// tells the manager on the game thread when a world finished loading and when it was left, from LeviLamina's
// client level events
class WorldHooks {
public:
    static void hook() {
        auto& bus = ll::event::EventBus::getInstance();
        sJoin     = bus.emplaceListener<ll::event::ClientJoinLevelEvent>([](ll::event::ClientJoinLevelEvent&) {
            if (RustBridge::running()) amm_host_world(true);
        });
        sExit     = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](ll::event::ClientExitLevelEvent&) {
            if (RustBridge::running()) amm_host_world(false);
        });
    }

    static void unhook() {
        auto& bus = ll::event::EventBus::getInstance();
        if (sJoin) bus.removeListener<ll::event::ClientJoinLevelEvent>(sJoin);
        if (sExit) bus.removeListener<ll::event::ClientExitLevelEvent>(sExit);
        sJoin = nullptr;
        sExit = nullptr;
    }

private:
    static inline ll::event::ListenerPtr sJoin;
    static inline ll::event::ListenerPtr sExit;
};

} // namespace amm_shim

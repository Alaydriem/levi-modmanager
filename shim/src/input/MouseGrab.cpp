#include "input/MouseGrab.h"

#include "mod/StopLatch.h"

#include <ll/api/service/TargetedBedrock.h>
#include <mc/client/game/ClientInstance.h>
#include <mc/client/game/IMinecraftGame.h>

#include <atomic>

namespace amm_shim {

namespace {

std::atomic<bool> gCursorFree{false};
// game thread only: the cursor_free value apply() last acted on
bool gApplied = false;

} // namespace

void MouseGrab::setCursorFree(bool free) { gCursorFree.store(free, std::memory_order_relaxed); }

void MouseGrab::apply(ClientInstance& client) {
    if (StopLatch::raised()) return;
    bool free = gCursorFree.load(std::memory_order_relaxed);
    if (free == gApplied) return;
    gApplied = free;
    if (free) {
        client.getMinecraftGame().releaseMouse();
        return;
    }
    if (inPlay(client)) client.getMinecraftGame().grabMouse();
}

bool MouseGrab::blocksGrab() { return gCursorFree.load(std::memory_order_relaxed); }

void MouseGrab::restoreOnDisable(HWND gameWindow) {
    bool wasFree = gApplied;
    gCursorFree.store(false, std::memory_order_relaxed);
    gApplied = false;
    if (!wasFree || !gameWindow || GetForegroundWindow() != gameWindow) return;
    auto client = ll::service::getClientInstance();
    if (!client || !inPlay(*client)) return;
    client->getMinecraftGame().grabMouse();
}

bool MouseGrab::inPlay(ClientInstance& client) {
    return client.getLocalPlayer() != nullptr && client.isWorldActive() && !client.isShowingMenu();
}

} // namespace amm_shim

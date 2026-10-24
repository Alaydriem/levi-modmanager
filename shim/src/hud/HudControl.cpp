#include "hud/HudControl.h"

#include "mod/StopLatch.h"

#include <mc/client/game/ClientInstance.h>
#include <mc/client/options/IOptionRegistry.h>

#include <atomic>

namespace amm_shim {

namespace {

std::atomic<bool> gWanted{false};
// game thread only: the manager hides the HUD now, and the player's own value from before it did
bool gHidden        = false;
bool gPlayerHideHud = false;

} // namespace

void HudControl::setWanted(bool hide) { gWanted.store(hide, std::memory_order_relaxed); }

void HudControl::apply(ClientInstance& client) {
    if (StopLatch::raised()) return;
    bool wanted = gWanted.load(std::memory_order_relaxed);
    if (wanted == gHidden) return;
    IOptionRegistry& options = client.getOptions();
    if (wanted) {
        gPlayerHideHud = options.getHideHud();
        options.setHideGUI(true);
    } else {
        options.setHideGUI(gPlayerHideHud);
    }
    gHidden = wanted;
}

void HudControl::restore(ClientInstance& client) {
    gWanted.store(false, std::memory_order_relaxed);
    if (!gHidden) return;
    client.getOptions().setHideGUI(gPlayerHideHud);
    gHidden = false;
}

} // namespace amm_shim

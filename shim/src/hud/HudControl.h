#pragma once

class ClientInstance;

namespace amm_shim {

// hides the vanilla HUD while the host frame asks for it, and puts back the value the player had before
class HudControl {
public:
    // the hide_vanilla_hud answer of the last host frame; render thread
    static void setWanted(bool hide);

    // game thread, each client tick: applies a change of the wanted state to the game options
    static void apply(ClientInstance& client);

    // game thread, on disable: puts back the player's own value if the HUD is still hidden by the manager
    static void restore(ClientInstance& client);
};

} // namespace amm_shim

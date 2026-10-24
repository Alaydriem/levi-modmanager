#pragma once

#include <windows.h>

class ClientInstance;

namespace amm_shim {

// lets go of the mouse or takes it back for the game when the cursor_free answer of the host frame changes, and
// tells the grab hook when the game must not take the mouse back on its own
class MouseGrab {
public:
    // the cursor_free answer of the last host frame; render thread
    static void setCursorFree(bool free);

    // game thread, each client tick: releases the mouse when cursor_free turned on, and grabs it again when it
    // turned off while the game is in play
    static void apply(ClientInstance& client);

    // true while cursor_free is on: GrabMouseHook then skips the game's own grab
    static bool blocksGrab();

    // on disable with the cursor still free: grabs the mouse once if the game is in play (in a world, with a local
    // player and no menu) and `gameWindow` is the foreground window, so the player is not left with a free cursor
    // once the manager is gone. An unfocused game, as at game close, is left alone
    static void restoreOnDisable(HWND gameWindow);

private:
    static bool inPlay(ClientInstance& client);
};

} // namespace amm_shim

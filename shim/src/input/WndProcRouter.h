#pragma once

#include "alaydriem_mod_manager.h"

#include <windows.h>

#include <cstdint>

namespace amm_shim {

// the manager's one WndProc subclass on the overlay's game window. It sends pointer moves, buttons, the wheel,
// WM_CHAR and focus loss to Rust in back-buffer pixels, swallows each message Rust consumes, and answers
// WM_SETCURSOR from the last host frame while the cursor is free. Keys reach Rust through InputListeners instead
class WndProcRouter {
public:
    // pointer buttons by the index Rust uses: 0 left, 1 right, 2 middle; the wheel has its own slot after them
    static constexpr uint8_t BUTTON_LEFT   = 0;
    static constexpr uint8_t BUTTON_RIGHT  = 1;
    static constexpr uint8_t BUTTON_MIDDLE = 2;
    static constexpr uint8_t SLOT_WHEEL    = 3;
    static constexpr uint8_t SLOT_COUNT    = 4;

    // subclasses `window` once; render thread, when the overlay binds to the game window
    static void install(HWND window);

    // stops forwarding, restores the original WndProc when ours is still on top, and waits until no thread is
    // inside a call into Rust. False when a thread stayed inside one past the wait: Rust must then be left alive.
    // Safe to call more than once
    static bool uninstall();

    // the cursor state of the last host frame; render thread, WM_SETCURSOR reads it
    static void setCursor(bool free, AmmCursorShape shape);

    // true when Rust consumed the last press or release of `slot` that came through the window, so the game's
    // own copy of that event is cancelled too; any thread
    static bool consumed(uint8_t slot);

    // true once after the game window lost focus, so a reader that tracks held keys forgets them; any thread
    static bool takeFocusLost();
};

} // namespace amm_shim

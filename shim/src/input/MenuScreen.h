#pragma once

#include <cstdint>

class ClientInstance;

namespace amm_shim {

// which game screen is on top, as the host frame input reports it in `menu_screen`: 1 for the start (main menu)
// screen, 2 for the pause screen, 0 for any other screen or play. It only reads the game's top screen name; the
// manager decides what to show
class MenuScreen {
public:
    static constexpr uint8_t NONE  = 0;
    static constexpr uint8_t START = 1;
    static constexpr uint8_t PAUSE = 2;

    // game thread, each client tick: reads the top screen name and logs it at debug level once per change
    static void observe(ClientInstance& client);

    // the code observe noted last, NONE before the first tick; any thread, the host frame input carries it
    static uint8_t current();
};

} // namespace amm_shim

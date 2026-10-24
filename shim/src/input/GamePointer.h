#pragma once

namespace amm_shim {

// the game's own mouse position while a mod shows the game picture in a game viewport: the game hit-tests with
// the position in its mouse feed, not with the window message, so a pointer inside the viewport is moved to the
// matching place of the full window there. Rust decides where (amm_host_game_point); this only converts between
// client and back-buffer pixels. Input thread
class GamePointer {
public:
    // sets `x` and `y` (client pixels) of one mouse feed event from the real cursor when it is inside the
    // viewport; leaves them as they are otherwise. It never transforms the incoming values: a button event carries
    // the position the game already holds, so the point always comes from the real cursor
    static void apply(short& x, short& y);
};

} // namespace amm_shim

#pragma once

namespace amm_shim {

// the manager's one pair of LeviLamina input listeners. Keys go to Rust with amm_host_key and mouse movement as a
// pointer delta; a key or movement Rust consumes is cancelled, and a game button or wheel event is cancelled when
// Rust consumed its copy from the game window
class InputListeners {
public:
    static void registerListeners();
    static void unregisterListeners();
};

} // namespace amm_shim

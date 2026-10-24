#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace amm_shim {

// starts and stops the manager in Rust and gives it the LeviLamina console for its Warn and Error lines
class RustBridge {
public:
    // sets the console mirror, then starts the host with the manager's config folder, the game folder that holds
    // LeviLamina's `logs` folder, and the LeviLamina version. False when the host did not start
    static bool create(std::filesystem::path const& configDir, std::filesystem::path const& gameDir, std::string_view version);

    // stops the mirror and the host; call it only after every hook is gone and every call into Rust has drained
    static void destroy();

    // true between a create() that worked and destroy()
    static bool running();

    // writes one line to the LeviLamina console; the callback Rust's console mirror calls on the game thread
    static void toConsole(uint8_t level, uint8_t const* msg, uintptr_t len);

    // `path` as UTF-8 text
    static std::string pathText(std::filesystem::path const& path);
};

} // namespace amm_shim

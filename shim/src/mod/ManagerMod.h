#pragma once

#include "ll/api/mod/NativeMod.h"

#include <filesystem>
#include <string>

namespace amm_shim {

// the LeviLamina entry point: starts the manager in Rust, installs the overlay, input and world hooks, and takes
// them down again in the order that keeps every call into Rust drained before the host stops. load() runs under the
// OS loader lock, so it only records the folders and the version; never create or wait on threads there. enable()
// starts the host, which creates threads, before it installs the hooks
class ManagerMod {
public:
    static ManagerMod& getInstance();

    ManagerMod() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();

    bool enable();

    bool disable();

private:
    // the folder that holds LeviLamina's own `logs` folder: the game's executable folder, or the parent of the
    // mods root when only that one has a `logs` folder
    static std::filesystem::path gameDir();

    ll::mod::NativeMod& mSelf;
    // recorded by load(), read by enable()
    std::filesystem::path mGameDir;
    std::string           mVersion;
};

} // namespace amm_shim

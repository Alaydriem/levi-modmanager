#pragma once

#include "ll/api/mod/NativeMod.h"

#include <string>

namespace demo_shim {

// the LeviLamina entry point of the demo: registers the demo with the mod manager in Rust, ticks it from the client
// update, and unregisters it when the mod stops. load() runs under the OS loader lock, so it only records the config
// folder; never create or wait on threads there. enable() registers, after the manager is enabled
class DemoMod {
public:
    static DemoMod& getInstance();

    DemoMod() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();

    bool enable();

    bool disable();

private:
    ll::mod::NativeMod& mSelf;
    // the absolute config folder as UTF-8, recorded by load()
    std::string mConfigDir;
    // false when the mod manager was missing at enable: the demo then stays inert until the next enable
    bool mRegistered = false;
};

} // namespace demo_shim

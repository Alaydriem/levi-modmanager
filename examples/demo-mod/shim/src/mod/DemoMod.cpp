#include "mod/DemoMod.h"

#include "bridge/ModLog.h"
#include "demo_ffi.h"
#include "hooks/ClientUpdateHook.h"

#include "ll/api/mod/RegisterHelper.h"

#include <filesystem>
#include <string>
#include <system_error>

namespace demo_shim {

namespace {

std::string pathText(std::filesystem::path const& path) {
    auto bytes = path.u8string();
    return std::string(reinterpret_cast<char const*>(bytes.data()), bytes.size());
}

// the mod's own config folder as an absolute path, which the mod manager requires
std::filesystem::path absoluteConfigDir(std::filesystem::path const& dir) {
    std::error_code       error;
    std::filesystem::path absolute = std::filesystem::absolute(dir, error);
    return error ? dir : absolute;
}

} // namespace

DemoMod& DemoMod::getInstance() {
    static DemoMod instance;
    return instance;
}

bool DemoMod::load() {
    mConfigDir = pathText(absoluteConfigDir(getSelf().getConfigDir()));
    return true;
}

bool DemoMod::enable() {
    DemoStatus status = demo_create(reinterpret_cast<uint8_t const*>(mConfigDir.data()), mConfigDir.size());
    if (status != DEMO_STATUS_OK) {
        // the manager's logger is not reachable without a registration, so this one line goes to the LeviLamina console
        if (status == DEMO_STATUS_MANAGER_MISSING) {
            getSelf().getLogger().error("alaydriem-mod-manager is not loaded, the demo stays inert");
        } else {
            getSelf().getLogger().error("the mod manager refused the demo, the demo stays inert");
        }
        return true;
    }
    mRegistered = true;
    ModLog::info("demo shim enabled", "{\"config_dir\":" + ModLog::quote(mConfigDir) + "}");
    ClientUpdateHook::hook();
    return true;
}

// the hook goes first, so no new tick enters Rust, then the demo unregisters
bool DemoMod::disable() {
    if (!mRegistered) return true;
    ClientUpdateHook::unhook();
    ModLog::info("demo shim unloaded");
    demo_destroy();
    mRegistered = false;
    return true;
}

} // namespace demo_shim

LL_REGISTER_MOD(demo_shim::DemoMod, demo_shim::DemoMod::getInstance());

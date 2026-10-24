#include "mod/ManagerMod.h"

#include "amm_host.h"
#include "bridge/RustBridge.h"
#include "bridge/RustLogger.h"
#include "hooks/ClientUpdateHook.h"
#include "hooks/GrabMouseHook.h"
#include "hooks/TurnDeltaHook.h"
#include "hooks/UIRenderHook.h"
#include "hooks/WorldHooks.h"
#include "hud/HudControl.h"
#include "input/InputListeners.h"
#include "input/MouseGrab.h"
#include "input/WndProcRouter.h"
#include "mod/StopLatch.h"
#include "render/OverlayHook.h"

#include "ll/api/Versions.h"
#include "ll/api/mod/Mod.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/service/GamingStatus.h"
#include "ll/api/service/TargetedBedrock.h"

#include <mc/client/game/ClientInstance.h>

#include <windows.h>

#include <format>
#include <string>
#include <string_view>
#include <system_error>

namespace amm_shim {

namespace {

constexpr std::string_view START_FAILED = "alaydriem-mod-manager could not start its Rust host";
constexpr DWORD            PATH_CHARS   = 32768;

bool hasLogs(std::filesystem::path const& folder) {
    std::error_code error;
    return !folder.empty() && std::filesystem::is_directory(folder / "logs", error);
}

std::filesystem::path executableDir() {
    std::wstring buffer(PATH_CHARS, L'\0');
    DWORD        length = GetModuleFileNameW(nullptr, buffer.data(), PATH_CHARS);
    if (length == 0 || length >= PATH_CHARS) return {};
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path modsRootParent() {
    std::error_code       error;
    std::filesystem::path root = std::filesystem::absolute(ll::mod::getModsRoot(), error);
    if (error) return {};
    return root.parent_path();
}

} // namespace

ManagerMod& ManagerMod::getInstance() {
    static ManagerMod instance;
    return instance;
}

std::filesystem::path ManagerMod::gameDir() {
    std::filesystem::path executable = executableDir();
    if (hasLogs(executable)) return executable;
    std::filesystem::path modsParent = modsRootParent();
    if (hasLogs(modsParent)) return modsParent;
    return executable.empty() ? modsParent : executable;
}

bool ManagerMod::load() {
    mGameDir = gameDir();
    mVersion = ll::getLoaderVersion().to_string();
    return true;
}

// the host starts before any hook, so no hook can call into Rust before it runs; after a disable that stopped the
// host this starts a new one, and a host left alive is kept
bool ManagerMod::enable() {
    if (!RustBridge::create(getSelf().getConfigDir(), mGameDir, mVersion)) {
        RustBridge::toConsole(AMM_LOG_LEVEL_ERROR, reinterpret_cast<uint8_t const*>(START_FAILED.data()), START_FAILED.size());
        return false;
    }
    RustLogger::info("manager shim enabled", std::format(R"({{"game_dir":{},"levilamina":{}}})", RustLogger::quote(RustBridge::pathText(mGameDir)), RustLogger::quote(mVersion)));
    StopLatch::lower();
    ClientUpdateHook::hook();
    GrabMouseHook::hook();
    TurnDeltaHook::hook();
    WorldHooks::hook();
    OverlayHook::arm();
    UIRenderHook::hook();
    InputListeners::registerListeners();
    return true;
}

// input and the grab hook go first, so the game gets its own WndProc and its mouse back while the host still runs;
// then the overlay drains and the tick stops; the cursor and the HUD are put back only after that, behind the stop
// latch; the host stops only when no thread can still be inside a call into Rust
bool ManagerMod::disable() {
    InputListeners::unregisterListeners();
    bool inputDrained = WndProcRouter::uninstall();
    GrabMouseHook::unhook();
    TurnDeltaHook::unhook();
    // the overlay forgets the window at shutdown
    HWND window = OverlayHook::gameWindow();
    UIRenderHook::unhook();
    bool overlayDrained = OverlayHook::shutdown();
    // a Present that bound the window during the first uninstall may have subclassed it again
    inputDrained = WndProcRouter::uninstall() && inputDrained;
    WorldHooks::unhook();
    ClientUpdateHook::unhook();
    StopLatch::raise();
    MouseGrab::restoreOnDisable(window);
    if (ll::getGamingStatus() != ll::GamingStatus::Stopping) {
        if (auto client = ll::service::getClientInstance()) HudControl::restore(*client);
    }
    // a thread still inside the overlay draw may be in amm_host_frame, or one inside the WndProc in a route call;
    // leaving the host alive at exit is safe
    if (!overlayDrained || !inputDrained) {
        RustLogger::warn("manager host left alive, the overlay or the window input did not stop in time");
        return true;
    }
    RustBridge::destroy();
    return true;
}

} // namespace amm_shim

LL_REGISTER_MOD(amm_shim::ManagerMod, amm_shim::ManagerMod::getInstance());

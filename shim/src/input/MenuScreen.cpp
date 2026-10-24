#include "input/MenuScreen.h"

#include "bridge/RustLogger.h"

#include <mc/client/game/ClientInstance.h>

#include <atomic>
#include <format>
#include <string>
#include <string_view>

namespace amm_shim {

namespace {

// the screen names of the start and pause screens as ClientInstance::getTopScreenName reports them
constexpr std::string_view START_SCREEN = "start_screen";
constexpr std::string_view PAUSE_SCREEN = "pause_screen";

std::atomic<uint8_t> gCurrent{MenuScreen::NONE};
// game thread only: the top screen name observe last saw, one string that is replaced, never grown
std::string gLastName;

uint8_t codeFor(std::string_view name) {
    if (name == START_SCREEN) return MenuScreen::START;
    if (name == PAUSE_SCREEN) return MenuScreen::PAUSE;
    return MenuScreen::NONE;
}

} // namespace

void MenuScreen::observe(ClientInstance& client) {
    std::string name = client.getTopScreenName();
    uint8_t     code = codeFor(name);
    gCurrent.store(code, std::memory_order_relaxed);
    if (name == gLastName) return;
    RustLogger::debug("top screen changed", std::format(R"({{"screen":{},"menu_screen":{}}})", RustLogger::quote(name), code));
    gLastName = std::move(name);
}

uint8_t MenuScreen::current() { return gCurrent.load(std::memory_order_relaxed); }

} // namespace amm_shim

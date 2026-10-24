#include "bridge/RustBridge.h"

#include "amm_host.h"
#include "mod/ManagerMod.h"

#include "ll/api/io/Logger.h"

#include <atomic>

namespace amm_shim {

namespace {

std::atomic<bool> gRunning{false};

uint8_t const* bytes(std::string const& text) { return reinterpret_cast<uint8_t const*>(text.data()); }

} // namespace

bool RustBridge::create(std::filesystem::path const& configDir, std::filesystem::path const& gameDir, std::string_view version) {
    amm_host_set_console(&RustBridge::toConsole);
    std::string config = pathText(configDir);
    std::string game   = pathText(gameDir);
    std::string text(version);
    AmmStatus   status = amm_host_create(bytes(config), config.size(), bytes(game), game.size(), bytes(text), text.size());
    gRunning.store(status == AMM_STATUS_OK, std::memory_order_seq_cst);
    return status == AMM_STATUS_OK;
}

void RustBridge::destroy() {
    gRunning.store(false, std::memory_order_seq_cst);
    amm_host_set_console(nullptr);
    amm_host_destroy();
}

bool RustBridge::running() { return gRunning.load(std::memory_order_seq_cst); }

void RustBridge::toConsole(uint8_t level, uint8_t const* msg, uintptr_t len) {
    if (!msg) return;
    std::string_view text(reinterpret_cast<char const*>(msg), len);
    auto&            logger = ManagerMod::getInstance().getSelf().getLogger();
    switch (level) {
    case AMM_LOG_LEVEL_ERROR: logger.error("{}", text); break;
    case AMM_LOG_LEVEL_WARN: logger.warn("{}", text); break;
    case AMM_LOG_LEVEL_INFO: logger.info("{}", text); break;
    default: logger.debug("{}", text); break;
    }
}

std::string RustBridge::pathText(std::filesystem::path const& path) {
    auto bytes = path.u8string();
    return std::string(reinterpret_cast<char const*>(bytes.data()), bytes.size());
}

} // namespace amm_shim

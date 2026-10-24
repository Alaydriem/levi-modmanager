#pragma once

#include "render/OverlayHook.h"

#include <ll/api/memory/Hook.h>
#include <mc/client/renderer/screen/MinecraftUIRenderContext.h>

namespace amm_shim {

// the first UI text flush means the swap chain exists: install the overlay hooks once
LL_TYPE_INSTANCE_HOOK(
    UIRenderHook,
    ll::memory::HookPriority::Normal,
    MinecraftUIRenderContext,
    &MinecraftUIRenderContext::$flushText,
    void,
    float                deltaTime,
    std::optional<float> obfuscateSwitchTime
) {
    origin(deltaTime, obfuscateSwitchTime);
    OverlayHook::init();
}

} // namespace amm_shim

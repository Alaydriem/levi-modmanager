#include "input/GamePointer.h"

#include "amm_host.h"
#include "render/OverlayHook.h"

#include <windows.h>

#include <cmath>

namespace amm_shim {

void GamePointer::apply(short& x, short& y) {
    HWND  window = OverlayHook::gameWindow();
    RECT  client{};
    POINT cursor{};
    if (!window || !GetClientRect(window, &client) || client.right <= 0 || client.bottom <= 0) return;
    if (!GetCursorPos(&cursor) || !ScreenToClient(window, &cursor)) return;
    float width  = 0.0f;
    float height = 0.0f;
    OverlayHook::backBufferSize(width, height);
    if (width <= 0.0f || height <= 0.0f) return;
    float toBufferX = width / static_cast<float>(client.right);
    float toBufferY = height / static_cast<float>(client.bottom);
    float gameX     = 0.0f;
    float gameY     = 0.0f;
    float bufferX   = static_cast<float>(cursor.x) * toBufferX;
    float bufferY   = static_cast<float>(cursor.y) * toBufferY;
    if (!amm_host_game_point(bufferX, bufferY, &gameX, &gameY)) return;
    x = static_cast<short>(std::lround(gameX / toBufferX));
    y = static_cast<short>(std::lround(gameY / toBufferY));
}

} // namespace amm_shim

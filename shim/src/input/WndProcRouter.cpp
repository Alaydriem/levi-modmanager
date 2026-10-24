#include "input/WndProcRouter.h"

#include "amm_host.h"
#include "bridge/RustLogger.h"
#include "render/InFlightCall.h"
#include "render/OverlayHook.h"

#include <windowsx.h>

#include <array>
#include <atomic>
#include <chrono>
#include <format>
#include <mutex>
#include <thread>

namespace amm_shim {

namespace {

constexpr float                     NOTCH = static_cast<float>(WHEEL_DELTA);
constexpr std::chrono::milliseconds DRAIN_TIMEOUT{2000};
constexpr std::chrono::milliseconds DRAIN_POLL{1};
constexpr uint32_t                  SURROGATE_HIGH_FIRST = 0xD800;
constexpr uint32_t                  SURROGATE_HIGH_LAST  = 0xDBFF;
constexpr uint32_t                  SURROGATE_LOW_FIRST  = 0xDC00;
constexpr uint32_t                  SURROGATE_LOW_LAST   = 0xDFFF;
constexpr uint32_t                  SURROGATE_BASE       = 0x10000;
constexpr int                       SURROGATE_SHIFT      = 10;

// guards install and uninstall; the WndProc itself reads only the atomics
std::mutex                                             gMutex;
HWND                                                   gWindow     = nullptr;
bool                                                   gSubclassed = false;
std::atomic<WNDPROC>                                   gOriginal{nullptr};
std::atomic<bool>                                      gForwarding{false};
std::atomic<bool>                                      gCursorFree{false};
std::atomic<uint8_t>                                   gCursorShape{AMM_CURSOR_SHAPE_DEFAULT};
std::array<std::atomic<bool>, WndProcRouter::SLOT_COUNT> gConsumed{};
// set by the window thread on focus loss, taken by the input thread's key listener; release on the store pairs
// with acquire in the exchange of takeFocusLost()
std::atomic<bool>                                      gFocusLost{false};
// threads inside forward(), the only part that calls Rust. The drain waits on this one alone, so a modal loop in
// the game's own WndProc (a window drag) does not hold up shutdown
std::atomic<int>                                       gRustCalls{0};
std::atomic<bool>                                      gThreadLogged{false};
HCURSOR                                                gArrow = nullptr;
HCURSOR                                                gHand  = nullptr;
HCURSOR                                                gText  = nullptr;
// this thread's own calls inside forward(), so an uninstall run from inside one does not wait for itself
thread_local int tDepth = 0;
// window thread only: the high half of a UTF-16 pair that WM_CHAR gave without its low half yet
uint32_t gHighSurrogate = 0;

// window messages give client-area pixels and the layers lay out in back-buffer pixels; they differ when the game
// renders at another size than its client area (another fullscreen mode, or the frames before ResizeBuffers)
void toBackBuffer(HWND hwnd, float& x, float& y) {
    float width  = 0.0f;
    float height = 0.0f;
    OverlayHook::backBufferSize(width, height);
    RECT client{};
    if (width <= 0.0f || height <= 0.0f || !GetClientRect(hwnd, &client)) return;
    if (client.right <= 0 || client.bottom <= 0) return;
    x *= width / static_cast<float>(client.right);
    y *= height / static_cast<float>(client.bottom);
}

bool sendPointer(HWND hwnd, AmmHostPointerKind kind, LPARAM lParam, uint8_t button, bool down) {
    float x = static_cast<float>(GET_X_LPARAM(lParam));
    float y = static_cast<float>(GET_Y_LPARAM(lParam));
    toBackBuffer(hwnd, x, y);
    return amm_host_pointer(kind, x, y, button, down, 0.0f).consume;
}

bool sendButton(HWND hwnd, LPARAM lParam, uint8_t button, bool down) {
    bool consume = sendPointer(hwnd, AMM_HOST_POINTER_KIND_BUTTON, lParam, button, down);
    gConsumed[button].store(consume, std::memory_order_relaxed);
    return consume;
}

bool sendWheel(WPARAM wParam) {
    float lines   = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / NOTCH;
    bool  consume = amm_host_pointer(AMM_HOST_POINTER_KIND_WHEEL, 0.0f, 0.0f, 0, false, lines).consume;
    gConsumed[WndProcRouter::SLOT_WHEEL].store(consume, std::memory_order_relaxed);
    return consume;
}

// WM_CHAR gives UTF-16 units; a pair becomes one code point, and a lone half is dropped
bool sendChar(WPARAM wParam) {
    auto unit = static_cast<uint32_t>(wParam);
    if (unit >= SURROGATE_HIGH_FIRST && unit <= SURROGATE_HIGH_LAST) {
        gHighSurrogate = unit;
        return false;
    }
    uint32_t ch = unit;
    if (unit >= SURROGATE_LOW_FIRST && unit <= SURROGATE_LOW_LAST) {
        if (gHighSurrogate == 0) return false;
        ch = SURROGATE_BASE + ((gHighSurrogate - SURROGATE_HIGH_FIRST) << SURROGATE_SHIFT) + (unit - SURROGATE_LOW_FIRST);
    }
    gHighSurrogate = 0;
    return amm_host_char(ch).consume;
}

bool forwardMouse(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, bool& consume) {
    switch (msg) {
    case WM_MOUSEMOVE: consume = sendPointer(hwnd, AMM_HOST_POINTER_KIND_MOVE, lParam, 0, false); return true;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: consume = sendButton(hwnd, lParam, WndProcRouter::BUTTON_LEFT, true); return true;
    case WM_LBUTTONUP: consume = sendButton(hwnd, lParam, WndProcRouter::BUTTON_LEFT, false); return true;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK: consume = sendButton(hwnd, lParam, WndProcRouter::BUTTON_RIGHT, true); return true;
    case WM_RBUTTONUP: consume = sendButton(hwnd, lParam, WndProcRouter::BUTTON_RIGHT, false); return true;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK: consume = sendButton(hwnd, lParam, WndProcRouter::BUTTON_MIDDLE, true); return true;
    case WM_MBUTTONUP: consume = sendButton(hwnd, lParam, WndProcRouter::BUTTON_MIDDLE, false); return true;
    case WM_MOUSEWHEEL: consume = sendWheel(wParam); return true;
    default: return false;
    }
}

// true when Rust consumed the message, so the game's own WndProc must not see it
bool forwardMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    bool consume = false;
    if (forwardMouse(hwnd, msg, wParam, lParam, consume)) return consume;
    switch (msg) {
    case WM_CHAR: return sendChar(wParam);
    case WM_KILLFOCUS:
        gHighSurrogate = 0;
        gFocusLost.store(true, std::memory_order_release);
        amm_host_focus_lost();
        return false;
    default: return false;
    }
}

bool isForwarded(UINT msg) {
    switch (msg) {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
    case WM_MBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_CHAR:
    case WM_KILLFOCUS: return true;
    default: return false;
    }
}

// counted for the drain; forwarding is checked again inside the count, so no call starts after uninstall saw 0
bool forward(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    InFlightCall guard(gRustCalls);
    ++tDepth;
    bool consume = gForwarding.load() && forwardMessage(hwnd, msg, wParam, lParam);
    --tDepth;
    return consume;
}

void noteWindowThread() {
    if (gThreadLogged.load(std::memory_order_relaxed) || gThreadLogged.exchange(true)) return;
    RustLogger::info("game window procedure runs on its thread", std::format(R"({{"thread":{}}})", GetCurrentThreadId()));
}

LRESULT passOn(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WNDPROC original = gOriginal.load();
    if (!original) return DefWindowProcW(hwnd, msg, wParam, lParam);
    return CallWindowProcW(original, hwnd, msg, wParam, lParam);
}

HCURSOR cursorFor(uint8_t shape) {
    if (shape == AMM_CURSOR_SHAPE_HAND) return gHand;
    if (shape == AMM_CURSOR_SHAPE_TEXT) return gText;
    return gArrow;
}

LRESULT route(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    noteWindowThread();
    if (!gForwarding.load()) return passOn(hwnd, msg, wParam, lParam);
    if (msg == WM_SETCURSOR && LOWORD(lParam) == HTCLIENT && gCursorFree.load(std::memory_order_relaxed)) {
        SetCursor(cursorFor(gCursorShape.load(std::memory_order_relaxed)));
        return TRUE;
    }
    if (isForwarded(msg) && forward(hwnd, msg, wParam, lParam)) return 0;
    return passOn(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK onWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) { return route(hwnd, msg, wParam, lParam); }

bool waitForDrain() {
    auto deadline = std::chrono::steady_clock::now() + DRAIN_TIMEOUT;
    while (gRustCalls.load() > tDepth) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(DRAIN_POLL);
    }
    return true;
}

// another subclass installed after ours would lose its place if ours were restored under it
bool restoreOriginal() {
    if (!IsWindow(gWindow)) return true;
    auto current = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(gWindow, GWLP_WNDPROC));
    if (current != &onWndProc) return false;
    SetWindowLongPtrW(gWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(gOriginal.load()));
    return true;
}

void clearConsumed() {
    for (auto& slot : gConsumed) slot.store(false, std::memory_order_relaxed);
}

} // namespace

void WndProcRouter::install(HWND window) {
    std::scoped_lock lock(gMutex);
    if (!window) return;
    if (gSubclassed) {
        // the WndProc is still in place from before (see uninstall); only forwarding stops and starts
        if (window == gWindow) gForwarding.store(true);
        return;
    }
    gArrow = LoadCursorW(nullptr, IDC_ARROW);
    gHand  = LoadCursorW(nullptr, IDC_HAND);
    gText  = LoadCursorW(nullptr, IDC_IBEAM);
    // a message that arrives between the swap and the store below still finds the original
    gOriginal.store(reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window, GWLP_WNDPROC)));
    LONG_PTR previous = SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&onWndProc));
    if (previous == 0) {
        RustLogger::warn("could not subclass the game window, layers get no mouse input", std::format(R"({{"error":{}}})", GetLastError()));
        gOriginal.store(nullptr);
        return;
    }
    gOriginal.store(reinterpret_cast<WNDPROC>(previous));
    gWindow     = window;
    gSubclassed = true;
    gForwarding.store(true);
    RustLogger::info("game window subclassed");
}

bool WndProcRouter::uninstall() {
    std::scoped_lock lock(gMutex);
    gForwarding.store(false);
    gCursorFree.store(false, std::memory_order_relaxed);
    clearConsumed();
    if (gSubclassed) {
        if (restoreOriginal()) {
            gSubclassed = false;
            gWindow     = nullptr;
        } else {
            RustLogger::warn("another window procedure sits above the manager's, the manager's stays and only passes messages on");
        }
    }
    if (waitForDrain()) return true;
    RustLogger::warn("a thread is still inside a call into Rust after the input drain");
    return false;
}

void WndProcRouter::setCursor(bool free, AmmCursorShape shape) {
    gCursorShape.store(static_cast<uint8_t>(shape), std::memory_order_relaxed);
    gCursorFree.store(free, std::memory_order_relaxed);
}

bool WndProcRouter::takeFocusLost() { return gFocusLost.exchange(false, std::memory_order_acquire); }

bool WndProcRouter::consumed(uint8_t slot) { return slot < SLOT_COUNT && gConsumed[slot].load(std::memory_order_relaxed); }

} // namespace amm_shim

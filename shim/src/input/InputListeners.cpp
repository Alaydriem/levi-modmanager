#include "input/InputListeners.h"

#include "amm_host.h"
#include "bridge/RustBridge.h"
#include "bridge/RustLogger.h"
#include "input/GamePointer.h"
#include "input/WndProcRouter.h"

#include <windows.h>

#include <ll/api/event/EventBus.h>
#include <ll/api/event/input/KeyInputEvent.h>
#include <ll/api/event/input/MouseInputEvent.h>

#include <atomic>
#include <bitset>
#include <cstdint>
#include <format>

namespace amm_shim {

namespace {

// MouseInputEvent::actionButtonId: 0 movement, 1 left, 2 right, 3 middle, 4 wheel
constexpr int MOUSE_MOVEMENT = 0;
constexpr int MOUSE_LEFT     = 1;
constexpr int MOUSE_RIGHT    = 2;
constexpr int MOUSE_MIDDLE   = 3;
constexpr int MOUSE_WHEEL    = 4;

constexpr uint32_t VK_SHIFT_KEY   = 0x10;
constexpr uint32_t VK_CONTROL_KEY = 0x11;
constexpr uint32_t VK_MENU_KEY    = 0x12;
constexpr uint8_t  MASK_CTRL      = 1;
constexpr uint8_t  MASK_SHIFT     = 2;
constexpr uint8_t  MASK_ALT       = 4;
constexpr SHORT    KEY_HELD       = static_cast<SHORT>(0x8000);
constexpr size_t   VK_COUNT       = 256;

ll::event::ListenerPtr gKeyListener;
ll::event::ListenerPtr gMouseListener;
// input thread only: the keys whose down went to Rust and whose up has not, so a second down is a repeat
std::bitset<VK_COUNT> gHeld;

// the left and right modifier keys become the plain ones, so a chord such as "Ctrl" matches either side
uint32_t normalise(uint32_t vk) {
    switch (vk) {
    case VK_LCONTROL:
    case VK_RCONTROL: return VK_CONTROL_KEY;
    case VK_LSHIFT:
    case VK_RSHIFT: return VK_SHIFT_KEY;
    case VK_LMENU:
    case VK_RMENU: return VK_MENU_KEY;
    default: return vk;
    }
}

bool held(int vk) { return (GetAsyncKeyState(vk) & KEY_HELD) != 0; }

uint8_t modifiers() {
    uint8_t mods = 0;
    if (held(VK_CONTROL)) mods |= MASK_CTRL;
    if (held(VK_SHIFT)) mods |= MASK_SHIFT;
    if (held(VK_MENU)) mods |= MASK_ALT;
    return mods;
}

// a focus loss released every key, so a key held before it is not held now
bool repeatOf(uint32_t vk, bool down) {
    if (WndProcRouter::takeFocusLost()) gHeld.reset();
    if (vk >= VK_COUNT) return false;
    bool repeat = down && gHeld.test(vk);
    gHeld.set(vk, down);
    return repeat;
}

void onKey(ll::event::KeyInputEvent& event) {
    if (!RustBridge::running()) return;
    uint32_t vk     = normalise(static_cast<uint32_t>(event.keyCode()));
    bool     down   = event.isDown();
    bool     repeat = repeatOf(vk, down);
    if (amm_host_key(vk, down, repeat, modifiers(), GetTickCount64()).consume) event.cancel();
}

// the window copy of a button or wheel event went through WndProcRouter, which kept Rust's answer
bool windowConsumed(int button) {
    switch (button) {
    case MOUSE_LEFT: return WndProcRouter::consumed(WndProcRouter::BUTTON_LEFT);
    case MOUSE_RIGHT: return WndProcRouter::consumed(WndProcRouter::BUTTON_RIGHT);
    case MOUSE_MIDDLE: return WndProcRouter::consumed(WndProcRouter::BUTTON_MIDDLE);
    case MOUSE_WHEEL: return WndProcRouter::consumed(WndProcRouter::SLOT_WHEEL);
    default: return false;
    }
}

void onMouse(ll::event::MouseInputEvent& event) {
    if (!RustBridge::running()) return;
    GamePointer::apply(event.x(), event.y());
    int button = static_cast<int>(event.actionButtonId());
    if (button == MOUSE_MOVEMENT) {
        float dx = static_cast<float>(event.dx());
        float dy = static_cast<float>(event.dy());
        if (amm_host_pointer(AMM_HOST_POINTER_KIND_DELTA, dx, dy, 0, false, 0.0f).consume) event.cancel();
        return;
    }
    if (windowConsumed(button)) event.cancel();
}

} // namespace

void InputListeners::registerListeners() {
    auto& bus      = ll::event::EventBus::getInstance();
    gKeyListener   = bus.emplaceListener<ll::event::KeyInputEvent>(&onKey);
    gMouseListener = bus.emplaceListener<ll::event::MouseInputEvent>(&onMouse);
}

void InputListeners::unregisterListeners() {
    auto& bus = ll::event::EventBus::getInstance();
    if (gKeyListener) {
        bus.removeListener<ll::event::KeyInputEvent>(gKeyListener);
        gKeyListener = nullptr;
    }
    if (gMouseListener) {
        bus.removeListener<ll::event::MouseInputEvent>(gMouseListener);
        gMouseListener = nullptr;
    }
    gHeld.reset();
}

} // namespace amm_shim

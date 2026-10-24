#include "render/OverlayHook.h"

#include <windows.h>

// these names clash with LeviLamina's own definitions
#define D3D12_FEATURE_DATA_D3D12_OPTIONS D3D12_FEATURE_DATA_D3D12_OPTIONS_LEGACY
#define D3D12_FEATURE_DATA_ARCHITECTURE  D3D12_FEATURE_DATA_ARCHITECTURE_LEGACY
#define D3D12_RAYTRACING_GEOMETRY_DESC   D3D12_RAYTRACING_GEOMETRY_DESC_LEGACY

#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#undef D3D12_FEATURE_DATA_D3D12_OPTIONS
#undef D3D12_FEATURE_DATA_ARCHITECTURE
#undef D3D12_RAYTRACING_GEOMETRY_DESC

#include "amm_host.h"
#include "bridge/RustBridge.h"
#include "bridge/RustLogger.h"
#include "hud/HudControl.h"
#include "input/MenuScreen.h"
#include "input/MouseGrab.h"
#include "input/WndProcRouter.h"
#include "mod/StopLatch.h"
#include "render/D3D11StateBackup.h"
#include "render/FrameCapture.h"
#include "render/GamePicture.h"
#include "render/InFlightCall.h"
#include "render/LayerRenderer.h"
#include "render/TextureRegistry.h"

#include "ll/api/memory/Hook.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <format>
#include <mutex>
#include <thread>

using Microsoft::WRL::ComPtr;

namespace amm_shim {

namespace {

using PresentFn       = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using Present1Fn      = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*, UINT, UINT, DXGI_PRESENT_PARAMETERS const*);
using ResizeBuffersFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using ResizeBuffers1Fn =
    HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT, UINT const*, IUnknown* const*);
using ExecuteCommandListsFn = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

constexpr int                       PRESENT_INDEX               = 8;
constexpr int                       RESIZE_BUFFERS_INDEX        = 13;
constexpr int                       PRESENT1_INDEX              = 22;
constexpr int                       RESIZE_BUFFERS1_INDEX       = 39;
constexpr int                       EXECUTE_COMMAND_LISTS_INDEX = 10;
constexpr UINT                      MAX_BACK_BUFFERS            = DXGI_MAX_SWAP_CHAIN_BUFFERS;
constexpr int                       PROBE_SIZE                  = 8;
constexpr wchar_t                   PROBE_CLASS[]               = L"AlaydriemModManagerOverlayProbe";
constexpr std::chrono::milliseconds DRAIN_TIMEOUT{2000};
constexpr std::chrono::milliseconds DRAIN_POLL{1};
constexpr int                       QUEUE_WAIT_LOG_FRAMES = 600;
constexpr float                     DEFAULT_DPI           = 96.0f;

PresentFn             gOriginalPresent        = nullptr;
Present1Fn            gOriginalPresent1       = nullptr;
ResizeBuffersFn       gOriginalResizeBuffers  = nullptr;
ResizeBuffers1Fn      gOriginalResizeBuffers1 = nullptr;
ExecuteCommandListsFn gOriginalExecute        = nullptr;

ll::memory::FuncPtr gPresentTarget        = nullptr;
ll::memory::FuncPtr gPresentDetour        = nullptr;
ll::memory::FuncPtr gPresent1Target       = nullptr;
ll::memory::FuncPtr gPresent1Detour       = nullptr;
ll::memory::FuncPtr gResizeBuffersTarget  = nullptr;
ll::memory::FuncPtr gResizeBuffersDetour  = nullptr;
ll::memory::FuncPtr gResizeBuffers1Target = nullptr;
ll::memory::FuncPtr gResizeBuffers1Detour = nullptr;
ll::memory::FuncPtr gExecuteTarget        = nullptr;
ll::memory::FuncPtr gExecuteDetour        = nullptr;

std::mutex        gInstallMutex;
std::atomic<bool> gAttempted{false};
std::atomic<bool> gInstalled{false};
std::atomic<bool> gShuttingDown{false};
std::atomic<int>  gInFlight{0};
// set when shutdown timed out and left the device alive; guarded by gInstallMutex
bool gLeaked = false;

// the first direct queue the game submits to; D3D11On12 draws through it. Holds one reference
std::atomic<ID3D12CommandQueue*> gQueue{nullptr};

// Present1 may reach Present inside DXGI; only the outermost call on a thread draws
thread_local int tPresentDepth = 0;

// render state below is touched only inside Present and ResizeBuffers of the bound window's chain,
// which the game never runs at the same time
// the device is ready and bound to the game window
bool                                                         gDeviceReady  = false;
bool                                                         gDeviceFailed = false;
// the layer renderer's shaders failed on this device; tried again only on a new device
bool                                                         gRendererFailed = false;
// the layer textures were dropped; Rust is told with amm_host_textures_lost before the next frame
bool                                                         gTexturesLostPending = false;
ComPtr<ID3D11Device>                                         gDevice;
ComPtr<ID3D11DeviceContext>                                  gContext;
ComPtr<ID3D11On12Device>                                     gDevice11On12;
IDXGISwapChain*                                              gTargetChain = nullptr;
UINT                                                         gTargetCount = 0;
std::array<ComPtr<ID3D11Resource>, MAX_BACK_BUFFERS>         gWrapped;
std::array<ComPtr<ID3D11RenderTargetView>, MAX_BACK_BUFFERS> gTargets;

// the window the overlay draws for, kept across a device rebuild; the device pointer is identity only,
// kept alive by gDevice or gDevice11On12, and set only while the device is ready
bool  gBound       = false;
HWND  gBoundWindow = nullptr;
void* gBoundDevice = nullptr;

int  gQueueWaitFrames = 0;
bool gQueueWaitLogged = false;

// gBoundWindow for readers on other threads
std::atomic<HWND> gGameWindow{nullptr};

// the host frame failed and that was logged; cleared by the next frame that works, so a failure that lasts writes
// one line, not one per Present
bool gFrameFailureLogged = false;

// the safe state (cursor not free, vanilla HUD shown) was published because no host frame could run; cleared by
// the next frame that works, so the game gets its mouse and HUD back once and not on every Present
bool gSafeStatePublished = false;

// one line with the render thread's id, for the live check of which thread runs what
std::atomic<bool> gRenderThreadLogged{false};

// the bound chain's back-buffer width in the high 32 bits and height in the low 32 bits, so a reader on
// another thread never sees half of a resize; 0 until a chain is bound
std::atomic<uint64_t> gBackBufferSize{0};

void logDeviceStep(char const* step) { RustLogger::info("overlay device step", std::format(R"({{"step":{}}})", RustLogger::quote(step))); }

constexpr int BACK_BUFFER_WIDTH_SHIFT = 32;

void recordBackBufferSize(DXGI_SWAP_CHAIN_DESC const& desc) {
    uint64_t packed = (static_cast<uint64_t>(desc.BufferDesc.Width) << BACK_BUFFER_WIDTH_SHIFT) | desc.BufferDesc.Height;
    gBackBufferSize.store(packed, std::memory_order_relaxed);
}

// the arguments to ResizeBuffers can be 0, meaning the window size; only the chain knows the size it chose
void recordBackBufferSize(IDXGISwapChain* chain) {
    DXGI_SWAP_CHAIN_DESC desc{};
    if (SUCCEEDED(chain->GetDesc(&desc))) recordBackBufferSize(desc);
}

void releaseQueue() {
    if (ID3D12CommandQueue* queue = gQueue.exchange(nullptr)) queue->Release();
}

// every reference to a back buffer must be gone before ResizeBuffers, or it fails
void releaseTargets() {
    for (UINT i = 0; i < MAX_BACK_BUFFERS; ++i) {
        gTargets[i].Reset();
        gWrapped[i].Reset();
    }
    gTargetCount = 0;
    gTargetChain = nullptr;
    if (gDevice11On12 && gContext) {
        gContext->ClearState();
        gContext->Flush();
    }
}

bool createWrappedTargets(IDXGISwapChain* chain, UINT bufferCount) {
    UINT count = bufferCount < MAX_BACK_BUFFERS ? bufferCount : MAX_BACK_BUFFERS;
    for (UINT i = 0; i < count; ++i) {
        ComPtr<ID3D12Resource> buffer;
        if (FAILED(chain->GetBuffer(i, IID_PPV_ARGS(&buffer)))) return false;
        D3D11_RESOURCE_FLAGS flags{};
        flags.BindFlags = D3D11_BIND_RENDER_TARGET;
        if (FAILED(gDevice11On12->CreateWrappedResource(
                buffer.Get(),
                &flags,
                D3D12_RESOURCE_STATE_PRESENT,
                D3D12_RESOURCE_STATE_PRESENT,
                IID_PPV_ARGS(&gWrapped[i])
            ))) {
            return false;
        }
        if (FAILED(gDevice->CreateRenderTargetView(gWrapped[i].Get(), nullptr, &gTargets[i]))) return false;
    }
    gTargetCount = count;
    return true;
}

// a D3D11 flip chain always shows its current back buffer as buffer 0
bool createNativeTarget(IDXGISwapChain* chain) {
    ComPtr<ID3D11Texture2D> buffer;
    if (FAILED(chain->GetBuffer(0, IID_PPV_ARGS(&buffer)))) return false;
    if (FAILED(gDevice->CreateRenderTargetView(buffer.Get(), nullptr, &gTargets[0]))) return false;
    gTargetCount = 1;
    return true;
}

bool createTargets(IDXGISwapChain* chain) {
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(chain->GetDesc(&desc))) return false;
    bool created = gDevice11On12 ? createWrappedTargets(chain, desc.BufferCount) : createNativeTarget(chain);
    if (!created) {
        releaseTargets();
        return false;
    }
    gTargetChain = chain;
    recordBackBufferSize(desc);
    return true;
}

void noteQueueWait() {
    if (gQueueWaitLogged || ++gQueueWaitFrames < QUEUE_WAIT_LOG_FRAMES) return;
    gQueueWaitLogged = true;
    RustLogger::warn("overlay waits for the game's D3D12 command queue, none captured yet");
}

// the queue must belong to the swap chain's device; a foreign one is dropped so the next submit can be captured
bool createDevice11On12(IDXGISwapChain* chain) {
    ID3D12CommandQueue* queue = gQueue.load();
    if (!queue) {
        noteQueueWait();
        return false;
    }
    ComPtr<ID3D12Device> chainDevice;
    ComPtr<ID3D12Device> queueDevice;
    if (FAILED(chain->GetDevice(IID_PPV_ARGS(&chainDevice)))) {
        gDeviceFailed = true;
        RustLogger::error("overlay swap chain has neither a D3D11 nor a D3D12 device");
        return false;
    }
    if (FAILED(queue->GetDevice(IID_PPV_ARGS(&queueDevice))) || queueDevice != chainDevice) {
        if (gQueue.compare_exchange_strong(queue, nullptr)) queue->Release();
        noteQueueWait();
        return false;
    }
    IUnknown* queues[] = {queue};
    logDeviceStep("creating device (D3D12 through D3D11On12)");
    if (FAILED(D3D11On12CreateDevice(
            chainDevice.Get(),
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr,
            0,
            queues,
            1,
            0,
            &gDevice,
            &gContext,
            nullptr
        ))
        || FAILED(gDevice.As(&gDevice11On12))) {
        gDeviceFailed = true;
        RustLogger::error("overlay could not create a D3D11On12 device");
        gContext.Reset();
        gDevice.Reset();
        return false;
    }
    return true;
}

// drops the layer textures, the game picture and the capture staging textures. Rust still thinks the renderer
// holds them and waits for the captures in flight, so it is told before the next frame
void releaseLayerTextures() {
    GamePicture::release();
    bool capturing = FrameCapture::release();
    if (LayerRenderer::ready() || capturing) gTexturesLostPending = true;
    TextureRegistry::clear();
}

// drops every layer device object, the game picture and the capture staging textures; used when the device goes
// away
void releaseLayerGpu() {
    GamePicture::release();
    bool capturing = FrameCapture::release();
    if (LayerRenderer::ready() || capturing) gTexturesLostPending = true;
    LayerRenderer::release();
}

// the renderer's device objects are made once per device, at the first Present on it
void ensureRenderer() {
    if (LayerRenderer::ready() || gRendererFailed || !gDevice) return;
    if (LayerRenderer::init(gDevice.Get())) return;
    gRendererFailed = true;
    RustLogger::error("overlay layer renderer could not start, no layers are drawn this session");
}

// drops the layer renderer, the targets, the D3D11On12 wrappers, the device and the queue
void releaseDevice() {
    releaseLayerGpu();
    releaseTargets();
    gDevice11On12.Reset();
    gContext.Reset();
    gDevice.Reset();
    releaseQueue();
    gDeviceFailed    = false;
    gDeviceReady     = false;
    gRendererFailed  = false;
    gBoundDevice     = nullptr;
    gQueueWaitFrames = 0;
    gQueueWaitLogged = false;
}

void releaseBinding() {
    releaseDevice();
    gBound       = false;
    gBoundWindow = nullptr;
    gGameWindow.store(nullptr);
    gBackBufferSize.store(0, std::memory_order_relaxed);
}

bool ensureDevice(IDXGISwapChain* chain, HWND window, void* device) {
    if (gDeviceReady) return true;
    if (gDeviceFailed) return false;
    if (!gDevice) {
        if (SUCCEEDED(chain->GetDevice(IID_PPV_ARGS(&gDevice)))) {
            logDeviceStep("creating device (D3D11)");
            gDevice->GetImmediateContext(&gContext);
        } else if (!createDevice11On12(chain)) {
            return false;
        }
    }
    WndProcRouter::install(window);
    gDeviceReady = true;
    gBound       = true;
    gBoundWindow = window;
    gGameWindow.store(window);
    gBoundDevice = device;
    logDeviceStep(gDevice11On12 ? "device ready (D3D12 through D3D11On12)" : "device ready (D3D11)");
    return true;
}

// the device pointer is returned for identity only; the reference taken here is released on return
void* chainDevice(IDXGISwapChain* chain) {
    ComPtr<ID3D11Device> device11;
    if (SUCCEEDED(chain->GetDevice(IID_PPV_ARGS(&device11)))) return device11.Get();
    ComPtr<ID3D12Device> device12;
    if (SUCCEEDED(chain->GetDevice(IID_PPV_ARGS(&device12)))) return device12.Get();
    return nullptr;
}

// the bound chain passes at once; another chain on another window is skipped; a new device on the bound window
// tears everything down and builds it again
bool acceptChain(IDXGISwapChain* chain) {
    if (chain == gTargetChain) return true;
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(chain->GetDesc(&desc))) return false;
    if (gBound && desc.OutputWindow != gBoundWindow) return false;
    void* device = chainDevice(chain);
    if (gDeviceReady && device != gBoundDevice) {
        RustLogger::info("overlay sees a new device on the game window, rebuilding the overlay device");
        releaseDevice();
    }
    if (!ensureDevice(chain, desc.OutputWindow, device)) return false;
    // a new or resized chain drops the layer textures too, so they come again from Rust
    releaseTargets();
    releaseLayerTextures();
    if (!createTargets(chain)) return false;
    ensureRenderer();
    return true;
}

void drawLayers(AmmHostFrame const& frame, uint32_t first, uint32_t end) {
    float width  = 0.0f;
    float height = 0.0f;
    OverlayHook::backBufferSize(width, height);
    LayerRenderer::draw(gContext.Get(), gDevice.Get(), frame, first, end, width, height);
}

void copyCaptures(ID3D11Resource* backBuffer, uint32_t const* sequences, uint32_t count) {
    FrameCapture::copy(gDevice.Get(), gContext.Get(), backBuffer, sequences, count);
}

// the game picture when a layer draws it, the game-only captures, and also the layer captures when the frame has
// no mod layer to draw before them
void copyBeforeLayers(ID3D11Resource* backBuffer, AmmHostFrame const& frame, uint32_t modEnd) {
    if (frame.game_picture) GamePicture::copy(gDevice.Get(), gContext.Get(), backBuffer);
    copyCaptures(backBuffer, frame.captures, frame.capture_count);
    if (modEnd == 0) copyCaptures(backBuffer, frame.layer_captures, frame.layer_capture_count);
}

// draws the mod layers, copies the layer captures, then draws the manager layer. The target is unbound while the
// back buffer is copied, so the copy does not read a bound output. With no layer capture to take between them,
// all layers are drawn in one pass
void drawLayerPass(ID3D11RenderTargetView* target, ID3D11Resource* backBuffer, AmmHostFrame const& frame, uint32_t modEnd) {
    gContext->OMSetRenderTargets(1, &target, nullptr);
    if (modEnd == 0 || frame.layer_capture_count == 0) {
        drawLayers(frame, 0, frame.layer_count);
        return;
    }
    drawLayers(frame, 0, modEnd);
    gContext->OMSetRenderTargets(0, nullptr, nullptr);
    copyCaptures(backBuffer, frame.layer_captures, frame.layer_capture_count);
    gContext->OMSetRenderTargets(1, &target, nullptr);
    drawLayers(frame, modEnd, frame.layer_count);
}

// the captures are copied from the acquired back buffer, around the mod layers drawn on it
void drawWrapped(UINT index, AmmHostFrame const& frame) {
    uint32_t        modEnd    = LayerRenderer::modLayerEnd(frame);
    ID3D11Resource* wrapped[] = {gWrapped[index].Get()};
    gDevice11On12->AcquireWrappedResources(wrapped, 1);
    copyBeforeLayers(wrapped[0], frame, modEnd);
    if (frame.layer_count > 0) {
        drawLayerPass(gTargets[index].Get(), wrapped[0], frame, modEnd);
        gContext->OMSetRenderTargets(0, nullptr, nullptr);
    }
    gDevice11On12->ReleaseWrappedResources(wrapped, 1);
    gContext->Flush();
}

// the D3D11 context is the game's own: put its render targets and pipeline state back after drawing. CopyResource
// binds nothing, so the copies made before the layers leave the game's state as it was
void drawNative(AmmHostFrame const& frame) {
    uint32_t               modEnd = LayerRenderer::modLayerEnd(frame);
    ComPtr<ID3D11Resource> backBuffer;
    if (frame.game_picture || frame.capture_count > 0 || frame.layer_capture_count > 0) gTargets[0]->GetResource(&backBuffer);
    copyBeforeLayers(backBuffer.Get(), frame, modEnd);
    if (frame.layer_count == 0) return;
    std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> saved{};
    ID3D11DepthStencilView*                                                     savedDepth = nullptr;
    gContext->OMGetRenderTargets(static_cast<UINT>(saved.size()), saved.data(), &savedDepth);
    {
        D3D11StateBackup state(gContext.Get());
        drawLayerPass(gTargets[0].Get(), backBuffer.Get(), frame, modEnd);
    }
    gContext->OMSetRenderTargets(static_cast<UINT>(saved.size()), saved.data(), savedDepth);
    for (ID3D11RenderTargetView* view : saved) {
        if (view) view->Release();
    }
    if (savedDepth) savedDepth->Release();
}

bool currentBackBuffer(IDXGISwapChain* chain, UINT& index) {
    index = 0;
    if (!gDevice11On12) return true;
    ComPtr<IDXGISwapChain3> chain3;
    if (FAILED(chain->QueryInterface(IID_PPV_ARGS(&chain3)))) return false;
    index = chain3->GetCurrentBackBufferIndex();
    return true;
}

double secondsNow() {
    static double const frequency = [] {
        LARGE_INTEGER value{};
        QueryPerformanceFrequency(&value);
        return static_cast<double>(value.QuadPart);
    }();
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return static_cast<double>(counter.QuadPart) / frequency;
}

// layer coordinates are back-buffer pixels, the same pixels the WndProc router sends pointer events in
AmmHostFrameInput hostFrameInput() {
    AmmHostFrameInput input{};
    uint64_t          packed  = gBackBufferSize.load(std::memory_order_relaxed);
    input.width               = static_cast<uint32_t>(packed >> BACK_BUFFER_WIDTH_SHIFT);
    input.height              = static_cast<uint32_t>(packed & UINT32_MAX);
    UINT dpi                  = gBoundWindow ? GetDpiForWindow(gBoundWindow) : 0;
    input.pixels_per_point    = dpi != 0 ? static_cast<float>(dpi) / DEFAULT_DPI : 1.0f;
    input.time_secs           = secondsNow();
    input.now_ms              = GetTickCount64();
    input.menu_screen         = MenuScreen::current();
    return input;
}

void noteRenderThread() {
    if (gRenderThreadLogged.exchange(true)) return;
    RustLogger::info("overlay frames run on the render thread", std::format(R"({{"thread":{}}})", GetCurrentThreadId()));
}

// true when the frame is usable; a failure is logged once until a frame works again
bool runHostFrame(AmmHostFrameInput const& input, AmmHostFrame& frame) {
    AmmStatus status = amm_host_frame(&input, &frame);
    if (status == AMM_STATUS_OK) {
        gFrameFailureLogged = false;
        return true;
    }
    if (!gFrameFailureLogged) {
        RustLogger::warn("host frame failed, no layers drawn until it works again", std::format(R"({{"status":{}}})", static_cast<int>(status)));
    }
    gFrameFailureLogged = true;
    return false;
}

// the state the game thread and the window thread apply comes from this frame
void publishFrameState(AmmHostFrame const& frame) {
    if (StopLatch::raised()) return;
    gSafeStatePublished = false;
    WndProcRouter::setCursor(frame.cursor_free, frame.cursor);
    MouseGrab::setCursorFree(frame.cursor_free);
    HudControl::setWanted(frame.hide_vanilla_hud);
}

// with no host frame the last published state would stay, and GrabMouseHook would keep the mouse from the game,
// so the game's own cursor and HUD are put back once
void publishSafeState() {
    if (StopLatch::raised() || gSafeStatePublished) return;
    gSafeStatePublished = true;
    WndProcRouter::setCursor(false, AMM_CURSOR_SHAPE_DEFAULT);
    MouseGrab::setCursorFree(false);
    HudControl::setWanted(false);
}

// amm_host_frame runs only when the frame can be drawn, so no texture change Rust hands over is lost; its layers
// are borrowed until the next amm_host_frame, so they are used only here. A frame with no layers and no capture of
// either set costs no D3D call
void renderFrame(IDXGISwapChain* chain) {
    if (!acceptChain(chain)) return;
    if (OverlayHook::shuttingDown()) return;
    if (!LayerRenderer::ready() || !RustBridge::running()) {
        publishSafeState();
        return;
    }
    UINT index = 0;
    if (!currentBackBuffer(chain, index) || index >= gTargetCount || !gTargets[index]) {
        publishSafeState();
        return;
    }
    noteRenderThread();
    if (gTexturesLostPending) {
        amm_host_textures_lost();
        gTexturesLostPending = false;
    }
    // copies made on earlier Presents are read and reported before this frame, which hands them on
    FrameCapture::poll(gContext.Get());
    AmmHostFrameInput input = hostFrameInput();
    AmmHostFrame      frame{};
    if (!runHostFrame(input, frame)) {
        publishSafeState();
        return;
    }
    publishFrameState(frame);
    LayerRenderer::beginFrame(frame);
    if (frame.layer_count == 0 && frame.capture_count == 0 && frame.layer_capture_count == 0) return;
    if (gDevice11On12) {
        drawWrapped(index, frame);
    } else {
        drawNative(frame);
    }
}

void drawOverlay(IDXGISwapChain* chain, UINT flags) {
    if (tPresentDepth != 1 || (flags & DXGI_PRESENT_TEST) != 0 || gShuttingDown.load()) return;
    renderFrame(chain);
}

bool isBoundChain(IDXGISwapChain* chain) { return chain != nullptr && chain == gTargetChain; }

// only the bound chain's targets and layer textures are ours to drop; other chains resize untouched
void releaseTargetsFor(IDXGISwapChain* chain) {
    if (!isBoundChain(chain)) return;
    releaseTargets();
    releaseLayerTextures();
}

HRESULT STDMETHODCALLTYPE onPresent(IDXGISwapChain* chain, UINT sync, UINT flags) {
    InFlightCall guard(gInFlight);
    ++tPresentDepth;
    drawOverlay(chain, flags);
    HRESULT result = gOriginalPresent(chain, sync, flags);
    --tPresentDepth;
    return result;
}

HRESULT STDMETHODCALLTYPE onPresent1(IDXGISwapChain1* chain, UINT sync, UINT flags, DXGI_PRESENT_PARAMETERS const* params) {
    InFlightCall guard(gInFlight);
    ++tPresentDepth;
    drawOverlay(chain, flags);
    HRESULT result = gOriginalPresent1(chain, sync, flags, params);
    --tPresentDepth;
    return result;
}

HRESULT STDMETHODCALLTYPE
onResizeBuffers(IDXGISwapChain* chain, UINT count, UINT width, UINT height, DXGI_FORMAT format, UINT flags) {
    InFlightCall guard(gInFlight);
    bool bound = isBoundChain(chain);
    releaseTargetsFor(chain);
    HRESULT result = gOriginalResizeBuffers(chain, count, width, height, format, flags);
    if (bound && SUCCEEDED(result)) recordBackBufferSize(chain);
    return result;
}

HRESULT STDMETHODCALLTYPE onResizeBuffers1(
    IDXGISwapChain3* chain,
    UINT             count,
    UINT             width,
    UINT             height,
    DXGI_FORMAT      format,
    UINT             flags,
    UINT const*      nodeMasks,
    IUnknown* const* presentQueues
) {
    InFlightCall guard(gInFlight);
    bool bound = isBoundChain(chain);
    releaseTargetsFor(chain);
    HRESULT result = gOriginalResizeBuffers1(chain, count, width, height, format, flags, nodeMasks, presentQueues);
    if (bound && SUCCEEDED(result)) recordBackBufferSize(chain);
    return result;
}

void STDMETHODCALLTYPE onExecuteCommandLists(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists) {
    InFlightCall guard(gInFlight);
    if (!gShuttingDown.load() && !gQueue.load() && queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
        ID3D12CommandQueue* expected = nullptr;
        queue->AddRef();
        if (!gQueue.compare_exchange_strong(expected, queue)) queue->Release();
    }
    gOriginalExecute(queue, count, lists);
}

template <class Fn>
bool hookEntry(
    char const*          name,
    void*                entry,
    Fn                   detour,
    ll::memory::FuncPtr& target,
    ll::memory::FuncPtr& detourOut,
    Fn&                  original
) {
    target     = ll::memory::toFuncPtr(reinterpret_cast<Fn>(entry));
    detourOut  = ll::memory::toFuncPtr(detour);
    int status = ll::memory::hook(target, detourOut, reinterpret_cast<ll::memory::FuncPtr*>(&original), ll::memory::HookPriority::Normal);
    if (status == 0) return true;
    RustLogger::warn("overlay hook failed", std::format(R"({{"entry":{},"status":{}}})", RustLogger::quote(name), status));
    target    = nullptr;
    detourOut = nullptr;
    return false;
}

void unhookEntry(ll::memory::FuncPtr& target, ll::memory::FuncPtr& detour) {
    if (target && detour) ll::memory::unhook(target, detour);
    target = nullptr;
    detour = nullptr;
}

// the swap chain vtable comes from a throwaway chain on a hidden window of our own, never the game's window.
// The four entries are one set: cached targets are safe only while both resize hooks are in place
bool hookSwapChain(HWND probe) {
    D3D_FEATURE_LEVEL    level = D3D_FEATURE_LEVEL_11_0;
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount       = 1;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow      = probe;
    desc.SampleDesc.Count  = 1;
    desc.Windowed          = TRUE;
    desc.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;
    ComPtr<ID3D11Device>    device;
    ComPtr<IDXGISwapChain>  chain;
    ComPtr<IDXGISwapChain3> chain3;
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &level, 1, D3D11_SDK_VERSION, &desc, &chain, &device, nullptr, nullptr))
        || FAILED(chain.As(&chain3))) {
        RustLogger::warn("overlay could not make the probe swap chain");
        return false;
    }
    void** vtable = *reinterpret_cast<void***>(chain3.Get());
    return hookEntry("Present", vtable[PRESENT_INDEX], &onPresent, gPresentTarget, gPresentDetour, gOriginalPresent)
        && hookEntry("Present1", vtable[PRESENT1_INDEX], &onPresent1, gPresent1Target, gPresent1Detour, gOriginalPresent1)
        && hookEntry(
            "ResizeBuffers",
            vtable[RESIZE_BUFFERS_INDEX],
            &onResizeBuffers,
            gResizeBuffersTarget,
            gResizeBuffersDetour,
            gOriginalResizeBuffers
        )
        && hookEntry(
            "ResizeBuffers1",
            vtable[RESIZE_BUFFERS1_INDEX],
            &onResizeBuffers1,
            gResizeBuffers1Target,
            gResizeBuffers1Detour,
            gOriginalResizeBuffers1
        );
}

// optional: without it the D3D11 path still draws, and the D3D12 path cannot
void hookCommandQueue() {
    ComPtr<ID3D12Device>       device;
    ComPtr<ID3D12CommandQueue> queue;
    D3D12_COMMAND_QUEUE_DESC   desc{};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))
        || FAILED(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue)))) {
        RustLogger::warn("overlay could not make the probe D3D12 queue, it can draw only on a D3D11 game");
        return;
    }
    void** vtable = *reinterpret_cast<void***>(queue.Get());
    if (!hookEntry("ExecuteCommandLists", vtable[EXECUTE_COMMAND_LISTS_INDEX], &onExecuteCommandLists, gExecuteTarget, gExecuteDetour, gOriginalExecute)) {
        RustLogger::warn("overlay has no queue hook, it can draw only on a D3D11 game");
    }
}

bool installHooks() {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = DefWindowProcW;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = PROBE_CLASS;
    RegisterClassExW(&wc);
    HWND probe = CreateWindowW(PROBE_CLASS, L"", WS_OVERLAPPEDWINDOW, 0, 0, PROBE_SIZE, PROBE_SIZE, nullptr, nullptr, wc.hInstance, nullptr);
    bool hooked = probe && hookSwapChain(probe);
    if (hooked) hookCommandQueue();
    if (probe) DestroyWindow(probe);
    UnregisterClassW(PROBE_CLASS, wc.hInstance);
    return hooked;
}

void unhookAll() {
    unhookEntry(gPresentTarget, gPresentDetour);
    unhookEntry(gPresent1Target, gPresent1Detour);
    unhookEntry(gResizeBuffersTarget, gResizeBuffersDetour);
    unhookEntry(gResizeBuffers1Target, gResizeBuffers1Detour);
    unhookEntry(gExecuteTarget, gExecuteDetour);
}

bool waitForDrain() {
    auto deadline = std::chrono::steady_clock::now() + DRAIN_TIMEOUT;
    while (gInFlight.load() > 0) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(DRAIN_POLL);
    }
    return true;
}

} // namespace

void OverlayHook::backBufferSize(float& width, float& height) {
    uint64_t packed = gBackBufferSize.load(std::memory_order_relaxed);
    width           = static_cast<float>(packed >> BACK_BUFFER_WIDTH_SHIFT);
    height          = static_cast<float>(packed & UINT32_MAX);
}

HWND OverlayHook::gameWindow() { return gGameWindow.load(); }

void OverlayHook::arm() {
    std::scoped_lock lock(gInstallMutex);
    if (!gInstalled.load() && !gLeaked) gAttempted.store(false);
}

bool OverlayHook::init() {
    if (gAttempted.load()) return gInstalled.load();
    std::scoped_lock lock(gInstallMutex);
    if (gAttempted.load()) return gInstalled.load();
    gAttempted.store(true);
    gShuttingDown.store(false);
    if (!installHooks()) {
        unhookAll();
        RustLogger::warn("overlay could not hook the swap chain, no layers are drawn this session");
        return false;
    }
    gInstalled.store(true);
    RustLogger::info("overlay swap chain hooks installed");
    return true;
}

bool OverlayHook::shuttingDown() { return gShuttingDown.load(); }

bool OverlayHook::shutdown() {
    std::scoped_lock lock(gInstallMutex);
    gAttempted.store(true);
    if (!gInstalled.load()) return !gLeaked;
    gShuttingDown.store(true);
    unhookAll();
    gInstalled.store(false);
    if (!waitForDrain()) {
        // freeing under a thread still inside a hook would crash; leaking at exit is safe
        gLeaked = true;
        RustLogger::warn("overlay leaves its device alive, a thread is still inside a swap chain hook");
        return false;
    }
    releaseBinding();
    return true;
}

} // namespace amm_shim

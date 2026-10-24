#include "render/FrameCapture.h"

#include "bridge/RustLogger.h"
#include "render/CaptureSlot.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <format>

using Microsoft::WRL::ComPtr;

namespace amm_shim {

namespace {

constexpr uint32_t BYTES_PER_PIXEL = 4;

std::array<CaptureSlot, AMM_HOST_MAX_CAPTURES> gSlots;

// a multisampled back buffer was logged once this session
bool gMultisampleLogged = false;

void logFailure(uint32_t sequence, char const* reason, HRESULT result) {
    RustLogger::warn(
        "screenshot copy failed",
        std::format(R"({{"sequence":{},"reason":{},"hresult":{}}})", sequence, RustLogger::quote(reason), static_cast<uint32_t>(result))
    );
}

// a report Rust no longer waits for (it timed out or the device was lost) answers InvalidArgument; nothing to do
void report(uint32_t sequence, bool ok, DXGI_FORMAT format) {
    amm_host_capture_done(sequence, ok ? 1 : 0, static_cast<uint32_t>(format));
}

void fail(uint32_t sequence, char const* reason, HRESULT result, DXGI_FORMAT format) {
    logFailure(sequence, reason, result);
    report(sequence, false, format);
}

void freeSlot(CaptureSlot& slot) {
    slot.busy     = false;
    slot.sequence = 0;
    slot.waited   = 0;
}

CaptureSlot* freeSlotOf() {
    for (CaptureSlot& slot : gSlots) {
        if (!slot.busy) return &slot;
    }
    return nullptr;
}

// the staging texture of `slot` matches `source`, made again when the size or format changed
bool ensureTexture(ID3D11Device* device, CaptureSlot& slot, D3D11_TEXTURE2D_DESC const& source, HRESULT& result) {
    result = S_OK;
    if (slot.texture && slot.width == source.Width && slot.height == source.Height && slot.format == source.Format) return true;
    slot.texture.Reset();
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width            = source.Width;
    desc.Height           = source.Height;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = source.Format;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags   = D3D11_CPU_ACCESS_READ;
    result                = device->CreateTexture2D(&desc, nullptr, &slot.texture);
    if (FAILED(result)) {
        slot.texture.Reset();
        return false;
    }
    slot.width  = source.Width;
    slot.height = source.Height;
    slot.format = source.Format;
    return true;
}

void copyOne(ID3D11Device* device, ID3D11DeviceContext* ctx, ID3D11Resource* backBuffer, ID3D11Texture2D* texture, uint32_t sequence) {
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    if (desc.SampleDesc.Count > 1) {
        if (!gMultisampleLogged) logFailure(sequence, "the back buffer is multisampled", E_NOTIMPL);
        gMultisampleLogged = true;
        report(sequence, false, desc.Format);
        return;
    }
    CaptureSlot* slot = freeSlotOf();
    if (!slot) {
        fail(sequence, "no free staging texture", E_OUTOFMEMORY, desc.Format);
        return;
    }
    HRESULT result = S_OK;
    if (!ensureTexture(device, *slot, desc, result)) {
        fail(sequence, "the staging texture could not be made", result, desc.Format);
        return;
    }
    ctx->CopyResource(slot->texture.Get(), backBuffer);
    slot->busy     = true;
    slot->sequence = sequence;
    slot->waited   = 0;
}

// the rows of the mapped copy into the buffer Rust owns; false when Rust gave no buffer or a pitch too short for
// a whole row, so a capture is never reported with part of each row left unfilled
bool copyRows(CaptureSlot const& slot, D3D11_MAPPED_SUBRESOURCE const& mapped) {
    uint32_t rowPitch = 0;
    uint8_t* buffer   = amm_host_capture_buffer(slot.sequence, slot.width, slot.height, &rowPitch);
    if (!buffer || !mapped.pData) return false;
    size_t rowBytes = size_t{slot.width} * BYTES_PER_PIXEL;
    if (mapped.RowPitch < rowBytes || rowPitch < rowBytes) return false;
    auto const* source = static_cast<uint8_t const*>(mapped.pData);
    for (UINT row = 0; row < slot.height; ++row) {
        std::memcpy(buffer + size_t{row} * rowPitch, source + size_t{row} * mapped.RowPitch, rowBytes);
    }
    return true;
}

void pollOne(ID3D11DeviceContext* ctx, CaptureSlot& slot) {
    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT                  result = ctx->Map(slot.texture.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
    if (result == DXGI_ERROR_WAS_STILL_DRAWING) {
        if (++slot.waited < FrameCapture::MAP_WAIT_FRAMES) return;
        fail(slot.sequence, "the copy was not ready in time", result, slot.format);
        freeSlot(slot);
        return;
    }
    if (FAILED(result)) {
        fail(slot.sequence, "the staging texture could not be mapped", result, slot.format);
        freeSlot(slot);
        return;
    }
    bool copied = copyRows(slot, mapped);
    ctx->Unmap(slot.texture.Get(), 0);
    if (copied) {
        report(slot.sequence, true, slot.format);
    } else {
        fail(slot.sequence, "no capture buffer from the manager", S_FALSE, slot.format);
    }
    freeSlot(slot);
}

} // namespace

void FrameCapture::copy(ID3D11Device* device, ID3D11DeviceContext* ctx, ID3D11Resource* backBuffer, uint32_t const* sequences, uint32_t count) {
    count = std::min<uint32_t>(count, AMM_HOST_MAX_CAPTURES);
    if (count == 0 || !sequences) return;
    ComPtr<ID3D11Texture2D> texture;
    bool usable = device && ctx && backBuffer && SUCCEEDED(backBuffer->QueryInterface(IID_PPV_ARGS(&texture)));
    for (uint32_t i = 0; i < count; ++i) {
        if (usable) {
            copyOne(device, ctx, backBuffer, texture.Get(), sequences[i]);
        } else {
            fail(sequences[i], "the back buffer is not a readable 2D texture", E_NOINTERFACE, DXGI_FORMAT_UNKNOWN);
        }
    }
}

void FrameCapture::poll(ID3D11DeviceContext* ctx) {
    if (!ctx) return;
    for (CaptureSlot& slot : gSlots) {
        if (slot.busy) pollOne(ctx, slot);
    }
}

bool FrameCapture::release() {
    bool inFlight = false;
    for (CaptureSlot& slot : gSlots) {
        inFlight = inFlight || slot.busy;
        freeSlot(slot);
        slot.texture.Reset();
        slot.width  = 0;
        slot.height = 0;
        slot.format = DXGI_FORMAT_UNKNOWN;
    }
    return inFlight;
}

} // namespace amm_shim

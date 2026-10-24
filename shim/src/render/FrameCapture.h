#pragma once

#include "amm_host.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Resource;

namespace amm_shim {

// copies the back buffer for the screenshots Rust asks for and hands the pixels to Rust, with no wait on the GPU.
// Render thread only, inside Present. A fixed pool of AMM_HOST_MAX_CAPTURES staging textures: copy() issues one
// CopyResource per sequence it is given, and poll() tries a non-blocking Map of each busy texture on each later
// Present. A mapped copy goes into the buffer of amm_host_capture_buffer and is reported with
// amm_host_capture_done in the same call. A copy that cannot be made, mapped within MAP_WAIT_FRAMES Presents, or
// written is reported with ok 0. A multisampled back buffer is not resolved: its captures are reported with ok 0
class FrameCapture {
public:
    static constexpr uint32_t MAP_WAIT_FRAMES = AMM_HOST_CAPTURE_MAX_FRAMES;

    // copies `backBuffer` into a free staging texture for each of the first `count` sequences at `sequences`: the
    // game-only set of a frame or its layer set, each at its own capture point. On D3D11On12 call it while the
    // wrapped back buffer is acquired
    static void copy(ID3D11Device* device, ID3D11DeviceContext* ctx, ID3D11Resource* backBuffer, uint32_t const* sequences, uint32_t count);

    // tries to read each busy staging texture without waiting, and reports each capture that is read or failed
    static void poll(ID3D11DeviceContext* ctx);

    // drops every staging texture, for a resize, a device loss or shutdown. True when a capture was in flight: the
    // caller then tells Rust with amm_host_textures_lost, which fails it
    static bool release();
};

} // namespace amm_shim

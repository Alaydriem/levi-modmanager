#pragma once

#include "amm_host.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace amm_shim {

// draws the layers of one AmmHostFrame with D3D11, bottom to top, in one or more ranges. Render thread only. Works
// on the game's own D3D11 device and on the D3D11On12 device of the D3D12 path; the caller binds the render target
// and keeps the context state it needs. Each draw command picks its pixel shader by `shader` and its sampler by
// `sampler`; a command with an unknown shader or sampler is skipped and logged once
class LayerRenderer {
public:
    // makes the fixed pipeline objects on `device`; false leaves nothing behind. The shaders compile once per
    // process, on the first call; later calls build only device objects from the kept bytecode
    static bool init(ID3D11Device* device);

    // true between a successful init() and release()
    static bool ready();

    // the once-per-frame part of drawing `frame`: the manager layer's open and close logs and the freed owners.
    // Call it once for each frame, before any draw() of that frame, also for a frame with no layers
    static void beginFrame(AmmHostFrame const& frame);

    // the index of the first manager layer of `frame`, or `layer_count` when it has none. The layers before it
    // are the mod layers
    static uint32_t modLayerEnd(AmmHostFrame const& frame);

    // applies the texture deltas of layers [first, end), draws them into the bound target of `width` x `height`
    // pixels, then frees the textures each of them lists. Called for one or more ranges of a frame, in layer order
    static void draw(
        ID3D11DeviceContext* ctx, ID3D11Device* device, AmmHostFrame const& frame, uint32_t first, uint32_t end, float width, float height
    );

    // re-arms the once-per-open logs of the manager layer; called for a frame that has no manager layer
    static void managerClosed();

    // drops every device object and every layer texture; the compiled bytecode stays
    static void release();
};

} // namespace amm_shim

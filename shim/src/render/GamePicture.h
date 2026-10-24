#pragma once

#include <cstdint>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Resource;
struct ID3D11ShaderResourceView;

namespace amm_shim {

// the live game picture a layer may draw (AMM_HOST_GAME_PICTURE_TEXTURE): one texture of the back buffer's size,
// filled with one GPU copy at a Present whose host frame asks for it, after the game has drawn and before any
// layer. Render thread only. The texture is made at the first copy, made again when the back buffer's size or
// format changes, and dropped on a resize or a device loss. An sRGB back buffer is viewed through its UNORM format,
// so the layer shader reads the same gamma-space bytes it writes. A multisampled back buffer gets no picture
class GamePicture {
public:
    // copies `backBuffer` into the picture texture. On D3D11On12 call it while the wrapped back buffer is acquired
    static void copy(ID3D11Device* device, ID3D11DeviceContext* ctx, ID3D11Resource* backBuffer);

    // the view of the last copy, or null when there is none
    static ID3D11ShaderResourceView* view();

    // drops the texture and its view, for a resize, a device loss or shutdown
    static void release();
};

} // namespace amm_shim

#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>

namespace amm_shim {

// one staging texture of the frame capture pool and the capture it holds. The texture is kept between captures of
// the same size and format and made again when either changes
struct CaptureSlot {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    UINT                                    width    = 0;
    UINT                                    height   = 0;
    DXGI_FORMAT                             format   = DXGI_FORMAT_UNKNOWN;
    uint32_t                                sequence = 0;
    // a copy is issued into the texture and not reported yet
    bool                                    busy = false;
    // the Presents the copy has waited for its non-blocking Map
    uint32_t                                waited = 0;
};

} // namespace amm_shim

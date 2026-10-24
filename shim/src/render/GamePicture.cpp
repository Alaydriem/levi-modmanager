#include "render/GamePicture.h"

#include "bridge/RustLogger.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <format>

using Microsoft::WRL::ComPtr;

namespace amm_shim {

namespace {

ComPtr<ID3D11Texture2D>          gTexture;
ComPtr<ID3D11ShaderResourceView> gView;
// the back buffer size and format the texture was made for
UINT        gWidth  = 0;
UINT        gHeight = 0;
DXGI_FORMAT gFormat = DXGI_FORMAT_UNKNOWN;
// set after the first failure to make the texture, so a failing device is logged once per resize
bool gFailureLogged = false;

// the format the picture is sampled as: CopyResource accepts a texture of the same format family, and the layer
// shader writes gamma-space colour, so an sRGB back buffer is read as UNORM
DXGI_FORMAT viewFormat(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8X8_UNORM;
    default: return format;
    }
}

void dropTexture() {
    gView.Reset();
    gTexture.Reset();
    gWidth  = 0;
    gHeight = 0;
    gFormat = DXGI_FORMAT_UNKNOWN;
}

bool matches(D3D11_TEXTURE2D_DESC const& source) {
    return gTexture && source.Width == gWidth && source.Height == gHeight && source.Format == gFormat;
}

bool create(ID3D11Device* device, D3D11_TEXTURE2D_DESC const& source) {
    dropTexture();
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width            = source.Width;
    desc.Height           = source.Height;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = viewFormat(source.Format);
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &gTexture))
        || FAILED(device->CreateShaderResourceView(gTexture.Get(), nullptr, &gView))) {
        dropTexture();
        if (!gFailureLogged) {
            gFailureLogged = true;
            RustLogger::warn(
                "game picture texture could not be made, layers that draw it show nothing",
                std::format(R"({{"width":{},"height":{},"format":{}}})", source.Width, source.Height, static_cast<int>(source.Format))
            );
        }
        return false;
    }
    gWidth  = source.Width;
    gHeight = source.Height;
    gFormat = source.Format;
    return true;
}

} // namespace

void GamePicture::copy(ID3D11Device* device, ID3D11DeviceContext* ctx, ID3D11Resource* backBuffer) {
    if (!device || !ctx || !backBuffer) return;
    ComPtr<ID3D11Texture2D> buffer;
    if (FAILED(backBuffer->QueryInterface(IID_PPV_ARGS(&buffer)))) return;
    D3D11_TEXTURE2D_DESC source{};
    buffer->GetDesc(&source);
    if (source.SampleDesc.Count != 1) return;
    if (!matches(source) && !create(device, source)) return;
    ctx->CopyResource(gTexture.Get(), backBuffer);
}

ID3D11ShaderResourceView* GamePicture::view() { return gView.Get(); }

void GamePicture::release() {
    dropTexture();
    gFailureLogged = false;
}

} // namespace amm_shim

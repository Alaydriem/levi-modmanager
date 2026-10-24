#include "render/LayerRenderer.h"

#include "bridge/RustLogger.h"
#include "render/TextureRegistry.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <iterator>
#include <string_view>

using Microsoft::WRL::ComPtr;

namespace amm_shim {

namespace {

// positions arrive in physical pixels; the constant buffer holds the target size to map them to clip space.
// Colours are premultiplied and in gamma space, blended into the UNORM target with no sRGB conversion
constexpr char SHADER_SOURCE[] = R"hlsl(
cbuffer Screen : register(b0) {
    float2 screenSize;
    float2 padding;
};

struct VertexIn {
    float2 pos : POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};

struct PixelIn {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};

PixelIn vs_main(VertexIn input) {
    PixelIn output;
    output.pos = float4(input.pos.x * 2.0 / screenSize.x - 1.0, 1.0 - input.pos.y * 2.0 / screenSize.y, 0.0, 1.0);
    output.uv  = input.uv;
    output.col = input.col;
    return output;
}

Texture2D    uiTexture : register(t0);
SamplerState uiSampler : register(s0);

float4 ps_main(PixelIn input) : SV_Target {
    return input.col * uiTexture.Sample(uiSampler, input.uv);
}

// the game picture: the alpha of a back buffer means nothing, so the picture is opaque
float4 ps_picture(PixelIn input) : SV_Target {
    return input.col * float4(uiTexture.Sample(uiSampler, input.uv).rgb, 1.0);
}
)hlsl";

// the pixel shaders a draw command can name, by index: DrawCmd::SHADER_EGUI, SHADER_USER and SHADER_PICTURE. A
// user texture is drawn as egui draws its own
constexpr std::array<char const*, 3> PIXEL_ENTRIES = {"ps_main", "ps_main", "ps_picture"};
constexpr size_t                     SHADER_COUNT  = PIXEL_ENTRIES.size();

constexpr UINT VERTEX_STRIDE    = sizeof(AmmVertex);
constexpr UINT INDEX_STRIDE     = sizeof(uint32_t);
constexpr UINT INITIAL_VERTICES = 16384;
constexpr UINT INITIAL_INDICES  = 32768;
constexpr UINT CONSTANT_FLOATS  = 4;
// 16 mods at 262,144 vertices each plus the manager fit far below this; a frame past it is not drawn
constexpr UINT MAX_BUFFER_ELEMENTS = 16u * 1024u * 1024u;

static_assert(sizeof(AmmVertex) == 20, "AmmVertex must match the input layout");

ComPtr<ID3DBlob>                                    gVertexCode;
std::array<ComPtr<ID3DBlob>, SHADER_COUNT>          gPixelCode;
bool                                                gCompileFailed = false;
ComPtr<ID3D11VertexShader>                          gVertexShader;
std::array<ComPtr<ID3D11PixelShader>, SHADER_COUNT> gPixelShaders;
ComPtr<ID3D11InputLayout>                           gLayout;
ComPtr<ID3D11Buffer>                                gConstants;
ComPtr<ID3D11BlendState>                            gBlend;
ComPtr<ID3D11RasterizerState>                       gRaster;
ComPtr<ID3D11DepthStencilState>                     gDepth;
ComPtr<ID3D11SamplerState>                          gLinear;
ComPtr<ID3D11SamplerState>                          gPoint;
ComPtr<ID3D11Buffer>                                gVertices;
ComPtr<ID3D11Buffer>                                gIndices;
UINT                                                gVertexCapacity = 0;
UINT                                                gIndexCapacity  = 0;
bool                                                gReady          = false;
bool                                                gShaderLogged   = false;
bool                                                gSamplerLogged  = false;
bool                                                gOversizeLogged = false;
// the manager layer's drawn and skipped lines were written for this open
bool gManagerDrawnLogged = false;
bool gManagerSkipLogged  = false;

// one line per panel open when a draw command of the manager layer is skipped
void noteManagerSkip(AmmHostLayer const& layer, char const* reason) {
    if (layer.owner != AmmHostLayer_MANAGER || gManagerSkipLogged) return;
    gManagerSkipLogged = true;
    RustLogger::info("manager draw skipped", std::format(R"({{"reason":{}}})", RustLogger::quote(reason)));
}

bool compile(char const* entry, char const* target, ComPtr<ID3DBlob>& code) {
    ComPtr<ID3DBlob> errors;
    HRESULT          result = D3DCompile(
        SHADER_SOURCE,
        sizeof(SHADER_SOURCE) - 1,
        "amm_layers",
        nullptr,
        nullptr,
        entry,
        target,
        D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0,
        &code,
        &errors
    );
    if (SUCCEEDED(result)) return true;
    std::string_view text = errors ? std::string_view(static_cast<char const*>(errors->GetBufferPointer()), errors->GetBufferSize())
                                   : std::string_view("no compiler output");
    RustLogger::error(
        "layer shader did not compile",
        std::format(R"({{"entry":{},"output":{}}})", RustLogger::quote(entry), RustLogger::quote(text))
    );
    return false;
}

// a compile error does not go away on a retry, so it is not tried again
bool ensureBytecode() {
    if (gVertexCode) return true;
    if (gCompileFailed) return false;
    ComPtr<ID3DBlob>                           vertexCode;
    std::array<ComPtr<ID3DBlob>, SHADER_COUNT> pixelCode;
    bool                                       compiled = compile("vs_main", "vs_4_0", vertexCode);
    for (size_t i = 0; compiled && i < SHADER_COUNT; ++i) compiled = compile(PIXEL_ENTRIES[i], "ps_4_0", pixelCode[i]);
    if (!compiled) {
        gCompileFailed = true;
        return false;
    }
    gVertexCode = vertexCode;
    gPixelCode  = pixelCode;
    return true;
}

bool createShaders(ID3D11Device* device) {
    if (!ensureBytecode()) return false;
    ID3DBlob* vertexCode = gVertexCode.Get();
    if (FAILED(device->CreateVertexShader(vertexCode->GetBufferPointer(), vertexCode->GetBufferSize(), nullptr, &gVertexShader))) {
        return false;
    }
    for (size_t i = 0; i < SHADER_COUNT; ++i) {
        ID3DBlob* pixelCode = gPixelCode[i].Get();
        if (FAILED(device->CreatePixelShader(pixelCode->GetBufferPointer(), pixelCode->GetBufferSize(), nullptr, &gPixelShaders[i]))) {
            return false;
        }
    }
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(AmmVertex, pos),   D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(AmmVertex, uv),    D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(AmmVertex, color), D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    return SUCCEEDED(device->CreateInputLayout(
        layout,
        static_cast<UINT>(std::size(layout)),
        vertexCode->GetBufferPointer(),
        vertexCode->GetBufferSize(),
        &gLayout
    ));
}

D3D11_SAMPLER_DESC samplerDesc(D3D11_FILTER filter) {
    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter         = filter;
    sampler.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sampler.MaxLOD         = D3D11_FLOAT32_MAX;
    return sampler;
}

bool createStates(ID3D11Device* device) {
    D3D11_BUFFER_DESC constants{};
    constants.ByteWidth      = CONSTANT_FLOATS * sizeof(float);
    constants.Usage          = D3D11_USAGE_DYNAMIC;
    constants.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    constants.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].BlendEnable           = TRUE;
    blend.RenderTarget[0].SrcBlend              = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_INV_DEST_ALPHA;
    blend.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ONE;
    blend.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode        = D3D11_FILL_SOLID;
    raster.CullMode        = D3D11_CULL_NONE;
    raster.ScissorEnable   = TRUE;
    raster.DepthClipEnable = TRUE;

    D3D11_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable    = FALSE;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc      = D3D11_COMPARISON_ALWAYS;
    depth.StencilEnable  = FALSE;

    D3D11_SAMPLER_DESC linear = samplerDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR);
    D3D11_SAMPLER_DESC point  = samplerDesc(D3D11_FILTER_MIN_MAG_MIP_POINT);

    return SUCCEEDED(device->CreateBuffer(&constants, nullptr, &gConstants))
        && SUCCEEDED(device->CreateBlendState(&blend, &gBlend))
        && SUCCEEDED(device->CreateRasterizerState(&raster, &gRaster))
        && SUCCEEDED(device->CreateDepthStencilState(&depth, &gDepth))
        && SUCCEEDED(device->CreateSamplerState(&linear, &gLinear))
        && SUCCEEDED(device->CreateSamplerState(&point, &gPoint));
}

// grows only: a buffer is made again, at least twice as large, when a frame needs more than it holds
bool ensureBuffer(ID3D11Device* device, ComPtr<ID3D11Buffer>& buffer, UINT& capacity, size_t needed, UINT stride, UINT bind, UINT initial) {
    if (buffer && needed <= capacity) return true;
    if (needed > MAX_BUFFER_ELEMENTS) return false;
    UINT grown = std::max({static_cast<UINT>(needed), initial, std::min(capacity * 2, MAX_BUFFER_ELEMENTS)});
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth      = grown * stride;
    desc.Usage          = D3D11_USAGE_DYNAMIC;
    desc.BindFlags      = bind;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    buffer.Reset();
    capacity = 0;
    if (FAILED(device->CreateBuffer(&desc, nullptr, &buffer))) return false;
    capacity = grown;
    return true;
}

bool meshUsable(AmmMeshView const& mesh) {
    return mesh.vertices && mesh.indices && mesh.cmds && mesh.vertex_count != 0 && mesh.index_count != 0 && mesh.cmd_count != 0;
}

// one line per panel open when a frame first carries the manager layer
void noteManagerDrawn(AmmHostFrame const& frame) {
    for (uint32_t i = 0; i < frame.layer_count; ++i) {
        AmmHostLayer const& layer = frame.layers[i];
        if (layer.owner != AmmHostLayer_MANAGER) continue;
        if (gManagerDrawnLogged) return;
        gManagerDrawnLogged = true;
        RustLogger::debug(
            "manager layer drawn",
            std::format(R"({{"vertices":{},"cmds":{},"usable":{}}})", layer.mesh.vertex_count, layer.mesh.cmd_count, meshUsable(layer.mesh))
        );
        return;
    }
    LayerRenderer::managerClosed();
}

void countGeometry(AmmHostFrame const& frame, uint32_t first, uint32_t end, size_t& vertices, size_t& indices) {
    vertices = 0;
    indices  = 0;
    for (uint32_t i = first; i < end; ++i) {
        AmmMeshView const& mesh = frame.layers[i].mesh;
        if (!meshUsable(mesh)) continue;
        vertices += mesh.vertex_count;
        indices  += mesh.index_count;
    }
}

// the vertices and indices of each layer in the range go into one shared pair of buffers, one after the other in
// layer order
bool uploadGeometry(ID3D11DeviceContext* ctx, AmmHostFrame const& frame, uint32_t first, uint32_t end) {
    D3D11_MAPPED_SUBRESOURCE vertexMap{};
    D3D11_MAPPED_SUBRESOURCE indexMap{};
    if (FAILED(ctx->Map(gVertices.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &vertexMap))) return false;
    if (FAILED(ctx->Map(gIndices.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &indexMap))) {
        ctx->Unmap(gVertices.Get(), 0);
        return false;
    }
    auto* vertexOut = static_cast<uint8_t*>(vertexMap.pData);
    auto* indexOut  = static_cast<uint8_t*>(indexMap.pData);
    for (uint32_t i = first; i < end; ++i) {
        AmmMeshView const& mesh = frame.layers[i].mesh;
        if (!meshUsable(mesh)) continue;
        size_t vertexBytes = mesh.vertex_count * VERTEX_STRIDE;
        size_t indexBytes  = mesh.index_count * INDEX_STRIDE;
        std::memcpy(vertexOut, mesh.vertices, vertexBytes);
        std::memcpy(indexOut, mesh.indices, indexBytes);
        vertexOut += vertexBytes;
        indexOut  += indexBytes;
    }
    ctx->Unmap(gIndices.Get(), 0);
    ctx->Unmap(gVertices.Get(), 0);
    return true;
}

bool uploadConstants(ID3D11DeviceContext* ctx, float width, float height) {
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(ctx->Map(gConstants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return false;
    float constants[CONSTANT_FLOATS] = {width, height, 0.0f, 0.0f};
    std::memcpy(mapped.pData, constants, sizeof(constants));
    ctx->Unmap(gConstants.Get(), 0);
    return true;
}

void bindPipeline(ID3D11DeviceContext* ctx, float width, float height) {
    D3D11_VIEWPORT viewport{0.0f, 0.0f, width, height, 0.0f, 1.0f};
    ctx->RSSetViewports(1, &viewport);
    ctx->RSSetState(gRaster.Get());
    UINT          stride   = VERTEX_STRIDE;
    UINT          offset   = 0;
    ID3D11Buffer* vertices = gVertices.Get();
    ctx->IASetInputLayout(gLayout.Get());
    ctx->IASetVertexBuffers(0, 1, &vertices, &stride, &offset);
    ctx->IASetIndexBuffer(gIndices.Get(), DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11Buffer* constants = gConstants.Get();
    ctx->VSSetShader(gVertexShader.Get(), nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, &constants);
    ctx->GSSetShader(nullptr, nullptr, 0);
    ctx->HSSetShader(nullptr, nullptr, 0);
    ctx->DSSetShader(nullptr, nullptr, 0);
    float const blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    ctx->OMSetBlendState(gBlend.Get(), blendFactor, 0xffffffffu);
    ctx->OMSetDepthStencilState(gDepth.Get(), 0);
}

// the clip rectangle in pixels, cut to the current target in case it was resized after the frame was composed;
// false when nothing of it is left
bool scissorFor(AmmDrawCmd const& cmd, float width, float height, D3D11_RECT& rect) {
    for (float edge : cmd.clip) {
        if (!std::isfinite(edge)) return false;
    }
    rect.left   = static_cast<LONG>(std::floor(std::clamp(cmd.clip[0], 0.0f, width)));
    rect.top    = static_cast<LONG>(std::floor(std::clamp(cmd.clip[1], 0.0f, height)));
    rect.right  = static_cast<LONG>(std::ceil(std::clamp(cmd.clip[2], 0.0f, width)));
    rect.bottom = static_cast<LONG>(std::ceil(std::clamp(cmd.clip[3], 0.0f, height)));
    return rect.right > rect.left && rect.bottom > rect.top;
}

bool cmdInRange(AmmDrawCmd const& cmd, AmmMeshView const& mesh) {
    return cmd.index_count != 0 && cmd.index_offset <= mesh.index_count && cmd.index_count <= mesh.index_count - cmd.index_offset;
}

ID3D11PixelShader* shaderFor(AmmDrawCmd const& cmd) {
    if (cmd.shader < SHADER_COUNT) return gPixelShaders[cmd.shader].Get();
    if (!gShaderLogged) RustLogger::warn("draw command names an unknown pixel shader, skipped", std::format(R"({{"shader":{}}})", cmd.shader));
    gShaderLogged = true;
    return nullptr;
}

ID3D11SamplerState* samplerFor(AmmDrawCmd const& cmd) {
    if (cmd.sampler == AMM_SAMPLER_KIND_LINEAR) return gLinear.Get();
    if (cmd.sampler == AMM_SAMPLER_KIND_NEAREST) return gPoint.Get();
    if (!gSamplerLogged) RustLogger::warn("draw command names an unknown sampler, skipped", std::format(R"({{"sampler":{}}})", cmd.sampler));
    gSamplerLogged = true;
    return nullptr;
}

void drawLayer(ID3D11DeviceContext* ctx, AmmHostLayer const& layer, UINT indexBase, INT vertexBase, float width, float height) {
    AmmMeshView const& mesh = layer.mesh;
    for (uintptr_t i = 0; i < mesh.cmd_count; ++i) {
        AmmDrawCmd const& cmd = mesh.cmds[i];
        if (!cmdInRange(cmd, mesh)) {
            noteManagerSkip(layer, "command index range outside the mesh");
            continue;
        }
        ID3D11PixelShader*  shader  = shaderFor(cmd);
        ID3D11SamplerState* sampler = samplerFor(cmd);
        if (!shader || !sampler) {
            noteManagerSkip(layer, "unknown shader or sampler");
            continue;
        }
        ID3D11ShaderResourceView* view = TextureRegistry::find(layer.owner, cmd.texture);
        if (!view) {
            noteManagerSkip(layer, "texture missing from the registry");
            continue;
        }
        D3D11_RECT rect{};
        if (!scissorFor(cmd, width, height, rect)) {
            noteManagerSkip(layer, "scissor empty after clamp");
            continue;
        }
        ctx->RSSetScissorRects(1, &rect);
        ctx->PSSetShader(shader, nullptr, 0);
        ctx->PSSetSamplers(0, 1, &sampler);
        ctx->PSSetShaderResources(0, 1, &view);
        ctx->DrawIndexed(cmd.index_count, indexBase + cmd.index_offset, vertexBase);
    }
}

void drawLayers(ID3D11DeviceContext* ctx, AmmHostFrame const& frame, uint32_t first, uint32_t end, float width, float height) {
    UINT indexBase  = 0;
    INT  vertexBase = 0;
    for (uint32_t i = first; i < end; ++i) {
        AmmHostLayer const& layer = frame.layers[i];
        if (!meshUsable(layer.mesh)) continue;
        drawLayer(ctx, layer, indexBase, vertexBase, width, height);
        indexBase  += static_cast<UINT>(layer.mesh.index_count);
        vertexBase += static_cast<INT>(layer.mesh.vertex_count);
    }
    // the texture view must not stay bound past the draw: a freed texture would live on through the binding
    ID3D11ShaderResourceView* none = nullptr;
    ctx->PSSetShaderResources(0, 1, &none);
}

void drawGeometry(ID3D11DeviceContext* ctx, ID3D11Device* device, AmmHostFrame const& frame, uint32_t first, uint32_t end, float width, float height) {
    if (width <= 0.0f || height <= 0.0f) return;
    size_t vertices = 0;
    size_t indices  = 0;
    countGeometry(frame, first, end, vertices, indices);
    if (vertices == 0 || indices == 0) return;
    if (!ensureBuffer(device, gVertices, gVertexCapacity, vertices, VERTEX_STRIDE, D3D11_BIND_VERTEX_BUFFER, INITIAL_VERTICES)
        || !ensureBuffer(device, gIndices, gIndexCapacity, indices, INDEX_STRIDE, D3D11_BIND_INDEX_BUFFER, INITIAL_INDICES)) {
        if (!gOversizeLogged) {
            RustLogger::warn(
                "frame geometry does not fit the layer buffers, frame not drawn",
                std::format(R"({{"vertices":{},"indices":{}}})", vertices, indices)
            );
        }
        gOversizeLogged = true;
        return;
    }
    if (!uploadGeometry(ctx, frame, first, end) || !uploadConstants(ctx, width, height)) return;
    bindPipeline(ctx, width, height);
    drawLayers(ctx, frame, first, end, width, height);
}

void applyTextures(ID3D11DeviceContext* ctx, ID3D11Device* device, AmmHostFrame const& frame, uint32_t first, uint32_t end) {
    for (uint32_t i = first; i < end; ++i) TextureRegistry::apply(ctx, device, frame.layers[i].owner, frame.layers[i].mesh);
}

void freeTextures(AmmHostFrame const& frame, uint32_t first, uint32_t end) {
    for (uint32_t i = first; i < end; ++i) TextureRegistry::freeListed(frame.layers[i].owner, frame.layers[i].mesh);
}

bool layersReadable(AmmHostFrame const& frame) { return frame.layer_count == 0 || frame.layers; }

} // namespace

bool LayerRenderer::init(ID3D11Device* device) {
    release();
    if (!device || !createShaders(device) || !createStates(device)) {
        release();
        return false;
    }
    gReady = true;
    return true;
}

bool LayerRenderer::ready() { return gReady; }

void LayerRenderer::beginFrame(AmmHostFrame const& frame) {
    if (!layersReadable(frame)) return;
    noteManagerDrawn(frame);
    TextureRegistry::freeOwners(frame.freed_owners);
}

uint32_t LayerRenderer::modLayerEnd(AmmHostFrame const& frame) {
    if (!layersReadable(frame)) return 0;
    for (uint32_t i = 0; i < frame.layer_count; ++i) {
        if (frame.layers[i].owner == AmmHostLayer_MANAGER) return i;
    }
    return frame.layer_count;
}

void LayerRenderer::draw(ID3D11DeviceContext* ctx, ID3D11Device* device, AmmHostFrame const& frame, uint32_t first, uint32_t end, float width, float height) {
    if (!gReady || !ctx || !device) return;
    if (!layersReadable(frame)) return;
    end = std::min(end, frame.layer_count);
    if (first >= end) return;
    applyTextures(ctx, device, frame, first, end);
    drawGeometry(ctx, device, frame, first, end, width, height);
    freeTextures(frame, first, end);
}

void LayerRenderer::managerClosed() {
    gManagerDrawnLogged = false;
    gManagerSkipLogged  = false;
    TextureRegistry::managerClosed();
}

void LayerRenderer::release() {
    TextureRegistry::clear();
    gVertices.Reset();
    gIndices.Reset();
    gVertexCapacity = 0;
    gIndexCapacity  = 0;
    gPoint.Reset();
    gLinear.Reset();
    gDepth.Reset();
    gRaster.Reset();
    gBlend.Reset();
    gConstants.Reset();
    gLayout.Reset();
    for (auto& shader : gPixelShaders) shader.Reset();
    gVertexShader.Reset();
    gReady = false;
}

} // namespace amm_shim

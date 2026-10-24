#include "render/TextureRegistry.h"

#include "amm_host.h"
#include "bridge/RustLogger.h"
#include "render/GamePicture.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <cstddef>
#include <format>
#include <unordered_map>

using Microsoft::WRL::ComPtr;

namespace amm_shim {

namespace {

constexpr UINT     BYTES_PER_PIXEL  = 4;
constexpr uint32_t OWNER_SLOT_COUNT = 32;
constexpr size_t   OWNER_COUNT      = 256;

using OwnerTextures = std::unordered_map<uint64_t, ComPtr<ID3D11ShaderResourceView>>;

// one map per owner byte, so freeing an owner drops its map and no id of one owner meets another's
std::array<OwnerTextures, OWNER_COUNT> gTextures;
size_t                                 gCount       = 0;
bool                                   gLimitLogged = false;
// the manager panel's refused upload was logged for this open
bool gManagerRefusalLogged = false;

// one line per panel open when an upload for the manager layer fails or is refused
void noteManagerRefusal(char const* reason, AmmTextureDelta const& delta) {
    if (gManagerRefusalLogged) return;
    gManagerRefusalLogged = true;
    RustLogger::debug(
        "manager texture refused",
        std::format(R"({{"reason":{},"id":{},"width":{},"height":{}}})", RustLogger::quote(reason), delta.id, delta.width, delta.height)
    );
}

// the pixels of `delta` inside the mesh bytes, or null when the delta does not fit them
uint8_t const* pixelsOf(AmmMeshView const& mesh, AmmTextureDelta const& delta) {
    if (!mesh.bytes || delta.width == 0 || delta.height == 0) return nullptr;
    uint64_t needed = uint64_t{delta.width} * delta.height * BYTES_PER_PIXEL;
    uint64_t end    = uint64_t{delta.byte_offset} + delta.byte_len;
    if (delta.byte_len < needed || end > mesh.byte_count) return nullptr;
    return mesh.bytes + delta.byte_offset;
}

bool createTexture(ID3D11Device* device, AmmTextureDelta const& delta, uint8_t const* pixels, ComPtr<ID3D11ShaderResourceView>& view) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width            = delta.width;
    desc.Height           = delta.height;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem     = pixels;
    data.SysMemPitch = delta.width * BYTES_PER_PIXEL;
    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(device->CreateTexture2D(&desc, &data, &texture))) return false;
    return SUCCEEDED(device->CreateShaderResourceView(texture.Get(), nullptr, &view));
}

// true when `owner` may add one more texture: the manager up to MAX_TEXTURES, the mods together up to
// MAX_TEXTURES - MANAGER_RESERVED
bool hasRoom(uint8_t owner) {
    if (owner == AmmHostLayer_MANAGER) return gCount < TextureRegistry::MAX_TEXTURES;
    size_t modCount = gCount - gTextures[AmmHostLayer_MANAGER].size();
    return modCount < TextureRegistry::MAX_TEXTURES - TextureRegistry::MANAGER_RESERVED;
}

// a full delta is create-or-replace: after amm_host_textures_lost the owner sends it again with the same id
void applyFull(ID3D11Device* device, uint8_t owner, AmmTextureDelta const& delta, uint8_t const* pixels) {
    OwnerTextures& textures = gTextures[owner];
    auto           found    = textures.find(delta.id);
    if (found == textures.end() && !hasRoom(owner)) {
        if (owner == AmmHostLayer_MANAGER) noteManagerRefusal("texture registry is full", delta);
        if (!gLimitLogged) {
            RustLogger::warn(
                "texture registry is full, extra textures are not drawn",
                std::format(R"({{"limit":{},"owner":{}}})", TextureRegistry::MAX_TEXTURES, owner)
            );
        }
        gLimitLogged = true;
        return;
    }
    ComPtr<ID3D11ShaderResourceView> view;
    if (!createTexture(device, delta, pixels, view)) {
        if (owner == AmmHostLayer_MANAGER) noteManagerRefusal("CreateTexture2D failed", delta);
        RustLogger::warn(
            "could not create a layer texture",
            std::format(R"({{"owner":{},"id":{},"width":{},"height":{}}})", owner, delta.id, delta.width, delta.height)
        );
        return;
    }
    if (found != textures.end()) {
        found->second = view;
    } else {
        textures.emplace(delta.id, view);
        ++gCount;
    }
}

void applyPartial(ID3D11DeviceContext* ctx, uint8_t owner, AmmTextureDelta const& delta, uint8_t const* pixels) {
    OwnerTextures& textures = gTextures[owner];
    auto           found    = textures.find(delta.id);
    if (found == textures.end()) return;
    ComPtr<ID3D11Resource> resource;
    found->second->GetResource(&resource);
    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(resource.As(&texture))) return;
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    if (uint64_t{delta.x} + delta.width > desc.Width || uint64_t{delta.y} + delta.height > desc.Height) return;
    D3D11_BOX box{delta.x, delta.y, 0, delta.x + delta.width, delta.y + delta.height, 1};
    ctx->UpdateSubresource(texture.Get(), 0, &box, pixels, delta.width * BYTES_PER_PIXEL, 0);
}

} // namespace

void TextureRegistry::apply(ID3D11DeviceContext* ctx, ID3D11Device* device, uint8_t owner, AmmMeshView const& mesh) {
    if (!mesh.deltas) return;
    for (uintptr_t i = 0; i < mesh.delta_count; ++i) {
        AmmTextureDelta const& delta  = mesh.deltas[i];
        uint8_t const*         pixels = pixelsOf(mesh, delta);
        if (!pixels) {
            if (owner == AmmHostLayer_MANAGER) noteManagerRefusal("delta pixels do not fit the mesh bytes", delta);
            continue;
        }
        if (delta.full != 0) {
            applyFull(device, owner, delta, pixels);
        } else {
            applyPartial(ctx, owner, delta, pixels);
        }
    }
}

void TextureRegistry::freeListed(uint8_t owner, AmmMeshView const& mesh) {
    if (!mesh.freed) return;
    OwnerTextures& textures = gTextures[owner];
    for (uintptr_t i = 0; i < mesh.freed_count; ++i) gCount -= textures.erase(mesh.freed[i]);
}

void TextureRegistry::freeOwners(uint32_t owners) {
    if (owners == 0) return;
    for (uint32_t owner = 0; owner < OWNER_SLOT_COUNT; ++owner) {
        if ((owners & (1u << owner)) == 0) continue;
        gCount -= gTextures[owner].size();
        gTextures[owner].clear();
    }
}

ID3D11ShaderResourceView* TextureRegistry::find(uint8_t owner, uint64_t id) {
    // Rust keeps a game picture command only in a layer of the Overlay holder
    if (id == AMM_HOST_GAME_PICTURE_TEXTURE) return GamePicture::view();
    OwnerTextures const& textures = gTextures[owner];
    auto                 found    = textures.find(id);
    return found == textures.end() ? nullptr : found->second.Get();
}

void TextureRegistry::managerClosed() { gManagerRefusalLogged = false; }

void TextureRegistry::clear() {
    for (OwnerTextures& textures : gTextures) textures.clear();
    gCount       = 0;
    gLimitLogged = false;
}

} // namespace amm_shim

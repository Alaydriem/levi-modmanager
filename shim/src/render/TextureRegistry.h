#pragma once

#include "alaydriem_mod_manager.h"

#include <cstdint>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;

namespace amm_shim {

// the GPU textures of every layer, keyed by the owner slot and the owner's own texture id, so one mod never
// reaches the textures of another. Render thread only. Holds at most MAX_TEXTURES entries; a new texture past
// that is not created. The mods together hold at most MAX_TEXTURES - MANAGER_RESERVED, so the manager's panel can
// always create its own. Entries leave when their owner frees them, when the owner's slot is freed, or on clear()
class TextureRegistry {
public:
    static constexpr size_t MAX_TEXTURES     = 8192;
    static constexpr size_t MANAGER_RESERVED = 64;

    // creates, replaces or updates the textures the deltas of `mesh` name for `owner`
    static void apply(ID3D11DeviceContext* ctx, ID3D11Device* device, uint8_t owner, AmmMeshView const& mesh);

    // drops the textures `mesh.freed` lists for `owner`; called after the frame is drawn
    static void freeListed(uint8_t owner, AmmMeshView const& mesh);

    // drops every texture of each owner whose bit is set in `owners` (bit N is slot N)
    static void freeOwners(uint32_t owners);

    // the view for `owner`'s texture `id`, or null when it does not exist. AMM_HOST_GAME_PICTURE_TEXTURE gives the
    // game picture (GamePicture), null before its first copy
    static ID3D11ShaderResourceView* find(uint8_t owner, uint64_t id);

    // re-arms the once-per-open log of a refused manager texture
    static void managerClosed();

    // drops every texture; the caller then tells Rust with amm_host_textures_lost
    static void clear();
};

} // namespace amm_shim

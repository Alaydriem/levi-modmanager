use amm_abi::types::AmmCursorShape;
use amm_core::structs::capture::PendingCaptures;
use amm_core::structs::mesh::GamePicture;

use super::AmmHostLayer;

/// The most captures one `AmmHostFrame` asks for, one per shim staging texture.
pub const AMM_HOST_MAX_CAPTURES: usize = 4;

const _: () = assert!(AMM_HOST_MAX_CAPTURES == PendingCaptures::CAPACITY);

/// The Presents a capture may take from the frame that offers it to `amm_host_capture_done`. The manager fails a
/// capture not reported by then, and the shim stops waiting on its staging copy after as many Presents.
pub const AMM_HOST_CAPTURE_MAX_FRAMES: u32 = 30;

/// The texture id of a draw command that shows the game picture (`GamePicture::HOST_TEXTURE`). The shim draws its
/// own copy of the back buffer for it; it is never in the texture registry.
pub const AMM_HOST_GAME_PICTURE_TEXTURE: u64 = 0x8000_0000_FFFF_FFFF;

const _: () = assert!(AMM_HOST_GAME_PICTURE_TEXTURE == GamePicture::HOST_TEXTURE);

/// What the shim draws for one Present: `layer_count` layers at `layers`, bottom to top, valid until the next
/// `amm_host_frame`. `freed_owners` has bit N set when mod slot N left since the last frame, so the shim drops
/// every texture of that owner. `cursor_free` releases the mouse grab and shows `cursor`; `hide_vanilla_hud`
/// hides the game HUD.
///
/// Two sets of screenshots are taken at this Present, at two capture points. The first `capture_count` entries of
/// `captures` are the game-only ones: the shim copies the back buffer into a staging texture for each after the
/// game has drawn and before it draws any layer. The first `layer_capture_count` entries of `layer_captures` are
/// the ones with mod layers: the shim draws `layers` up to the first layer whose owner is
/// `AmmHostLayer::MANAGER`, copies the back buffer for each, then draws the rest. The manager's layer (the panel
/// and the toasts) is always the last layer, so the copy shows the layers of all mods and not the manager's. With
/// no mod layer in the frame, the layer captures are copied at the same point as the game-only ones. Both sets
/// share the `AMM_HOST_MAX_CAPTURES` staging textures: together they hold at most that many sequences. Each
/// sequence is given once, in one set. When the copy can be read, the shim asks for a buffer with
/// `amm_host_capture_buffer`, copies the rows into it and calls `amm_host_capture_done`; a capture it cannot copy
/// it reports with `ok` 0. A capture not reported within 30 frames fails.
///
/// `game_picture` is true when a layer of this frame draws the game picture (`GamePicture::HOST_TEXTURE`). The
/// shim then copies the back buffer into its game picture texture at the same point as the game-only captures,
/// and draws that texture for the commands that name it. Only a mod that holds `Overlay` keeps such a command;
/// it is dropped from every other layer.
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct AmmHostFrame {
    pub layers: *const AmmHostLayer,
    pub layer_count: u32,
    pub freed_owners: u32,
    pub cursor_free: bool,
    pub cursor: AmmCursorShape,
    pub hide_vanilla_hud: bool,
    pub capture_count: u32,
    pub captures: [u32; AMM_HOST_MAX_CAPTURES],
    pub layer_capture_count: u32,
    pub layer_captures: [u32; AMM_HOST_MAX_CAPTURES],
    pub game_picture: bool,
}

impl AmmHostFrame {
    /// No layers, the cursor grabbed, the HUD shown, no capture and no game picture.
    pub fn empty() -> Self {
        Self {
            layers: std::ptr::null(),
            layer_count: 0,
            freed_owners: 0,
            cursor_free: false,
            cursor: AmmCursorShape::Default,
            hide_vanilla_hud: false,
            capture_count: 0,
            captures: [0; AMM_HOST_MAX_CAPTURES],
            layer_capture_count: 0,
            layer_captures: [0; AMM_HOST_MAX_CAPTURES],
            game_picture: false,
        }
    }

    /// This frame with the two capture sets of `offered` in place of its own.
    pub fn with_captures_of(self, offered: &AmmHostFrame) -> Self {
        Self {
            capture_count: offered.capture_count,
            captures: offered.captures,
            layer_capture_count: offered.layer_capture_count,
            layer_captures: offered.layer_captures,
            ..self
        }
    }
}

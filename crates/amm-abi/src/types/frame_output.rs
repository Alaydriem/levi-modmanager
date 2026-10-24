use super::AmmMeshView;

/// What a mod frame callback fills in. `layer_rect` is `[min_x, min_y, max_x, max_y]` in physical pixels.
/// `wants_pointer` and `wants_keyboard` are 0 or 1. `wants_keyboard` is 1 only while a text field of the
/// layer has keyboard focus, not for a focused button. `cursor` is an `AmmCursorShape` value.
/// `FrameReplyData::try_from(&AmmFrameOutput)` rejects any other value
/// with `AmmStatus::InvalidArgument`.
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct AmmFrameOutput {
    pub mesh: AmmMeshView,
    pub wants_pointer: u8,
    pub wants_keyboard: u8,
    pub cursor: u8,
    pub layer_rect: [f32; 4],
}

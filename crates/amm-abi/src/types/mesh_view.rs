use super::{AmmDrawCmd, AmmTextureDelta, AmmVertex};

/// A borrowed view of one egui pass. Every pointer stays valid until the next call of the same callback.
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct AmmMeshView {
    pub vertices: *const AmmVertex,
    pub vertex_count: usize,
    pub indices: *const u32,
    pub index_count: usize,
    pub cmds: *const AmmDrawCmd,
    pub cmd_count: usize,
    pub deltas: *const AmmTextureDelta,
    pub delta_count: usize,
    pub bytes: *const u8,
    pub byte_count: usize,
    pub freed: *const u64,
    pub freed_count: usize,
}

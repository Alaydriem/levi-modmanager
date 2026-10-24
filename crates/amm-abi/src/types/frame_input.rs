use super::AmmUiEvent;

/// What one mod egui pass receives. `events` stays valid for the duration of the frame callback.
///
/// `allowed_layers`: bit 0 Hud, bit 1 Overlay. From ABI 1.1 the host calls the frame callback once per layer of
/// the mod in each frame, and exactly one bit is set: the layer this call draws. A mod that holds both Hud and
/// Overlay is called twice, Hud first, and returns a separate mesh each time.
/// `input_flags`: bit 0 pointer, bit 1 keyboard, bit 2 navigation keys.
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct AmmFrameInput {
    pub width: u32,
    pub height: u32,
    pub pixels_per_point: f32,
    pub time_secs: f64,
    pub events: *const AmmUiEvent,
    pub event_count: usize,
    pub allowed_layers: u8,
    pub input_flags: u8,
}

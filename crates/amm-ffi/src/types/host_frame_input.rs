/// What the shim knows about the frame it is about to draw: the back buffer size in physical pixels, the scale,
/// the time in seconds for egui, the clock in milliseconds the manager uses for toasts and failures, and
/// `menu_screen`: the game screen on top, 1 for the start (main menu) screen, 2 for the pause screen and 0 for any
/// other screen or play. The manager shows its menu button only on the start and the pause screen.
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct AmmHostFrameInput {
    pub width: u32,
    pub height: u32,
    pub pixels_per_point: f32,
    pub time_secs: f64,
    pub now_ms: u64,
    pub menu_screen: u8,
}

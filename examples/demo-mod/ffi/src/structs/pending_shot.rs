use amm_core::structs::capture::ScreenshotLayers;

/// The one screenshot the demo waits on: the sequence the manager gave, the number in its file name, the capture
/// point it asked for, and whether the demo took the Hud lease only for this request.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct PendingShot {
    pub sequence: u32,
    pub number: u32,
    pub layers: ScreenshotLayers,
    pub temporary_hud: bool,
}

impl PendingShot {
    /// The file name of the screenshot: `demo-<n>.png` for the game only, `demo-layers-<n>.png` with mod layers.
    pub fn file_name(layers: ScreenshotLayers, number: u32) -> String {
        match layers {
            ScreenshotLayers::GameOnly => format!("demo-{number}.png"),
            ScreenshotLayers::WithModLayers => format!("demo-layers-{number}.png"),
        }
    }
}

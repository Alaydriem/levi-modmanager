/// A keybind of the demo, by its place in the descriptor's keybind list. The manager sends the place, not the
/// name.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DemoAction {
    ToggleCard,
    TakeCamera,
    OpenWindow,
    Screenshot,
    ScreenshotWithLayers,
}

impl DemoAction {
    /// The action at `index` of the descriptor's keybind list, or `None` past the end.
    pub fn from_index(index: u16) -> Option<DemoAction> {
        match index {
            0 => Some(DemoAction::ToggleCard),
            1 => Some(DemoAction::TakeCamera),
            2 => Some(DemoAction::OpenWindow),
            3 => Some(DemoAction::Screenshot),
            4 => Some(DemoAction::ScreenshotWithLayers),
            _ => None,
        }
    }

    /// The action name as the descriptor writes it.
    pub fn name(self) -> &'static str {
        match self {
            DemoAction::ToggleCard => "toggle_card",
            DemoAction::TakeCamera => "take_camera",
            DemoAction::OpenWindow => "open_window",
            DemoAction::Screenshot => "screenshot",
            DemoAction::ScreenshotWithLayers => "screenshot_layers",
        }
    }
}

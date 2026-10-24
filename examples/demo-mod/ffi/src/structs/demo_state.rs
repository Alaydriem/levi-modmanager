use amm_core::services::settings::ColorCodec;
use amm_core::structs::descriptor::ColorEncoding;
use egui::Color32;
use levi_ui::tokens::{ColorToken, color};

use super::DemoConfig;

/// What the card and the window show.
#[derive(Debug)]
pub struct DemoState {
    config: DemoConfig,
    accent: Color32,
    card_visible: bool,
    window_open: bool,
    show_last_event: bool,
    camera_line: String,
    screenshot_line: String,
    last_event: &'static str,
    name: String,
}

impl DemoState {
    pub fn new() -> Self {
        Self {
            config: DemoConfig::default(),
            accent: <color::Accent as ColorToken>::VALUE,
            card_visible: false,
            window_open: false,
            show_last_event: true,
            camera_line: "Camera: not held".to_string(),
            screenshot_line: String::new(),
            last_event: "none",
            name: String::new(),
        }
    }

    /// Takes a newly read config. An accent text that is not a hex colour keeps the Studio accent.
    pub fn apply_config(&mut self, config: DemoConfig) {
        let text = hcl::Value::String(config.card.accent.clone());
        self.accent = match ColorCodec::to_rgba(&text, ColorEncoding::HexString) {
            Some([r, g, b, a]) => Color32::from_rgba_unmultiplied(r, g, b, a),
            None => <color::Accent as ColorToken>::VALUE,
        };
        self.config = config;
    }

    pub fn config(&self) -> &DemoConfig {
        &self.config
    }

    pub fn accent(&self) -> Color32 {
        self.accent
    }

    pub fn card_visible(&self) -> bool {
        self.card_visible
    }

    pub fn set_card_visible(&mut self, visible: bool) {
        self.card_visible = visible;
    }

    pub fn window_open(&self) -> bool {
        self.window_open
    }

    pub fn set_window_open(&mut self, open: bool) {
        self.window_open = open;
    }

    pub fn show_last_event(&self) -> bool {
        self.show_last_event
    }

    pub fn set_show_last_event(&mut self, show: bool) {
        self.show_last_event = show;
    }

    pub fn camera_line(&self) -> &str {
        &self.camera_line
    }

    pub fn set_camera_line(&mut self, line: String) {
        self.camera_line = line;
    }

    /// The screenshot result line, empty until the first screenshot key press.
    pub fn screenshot_line(&self) -> &str {
        &self.screenshot_line
    }

    pub fn set_screenshot_line(&mut self, line: String) {
        self.screenshot_line = line;
    }

    /// The text of the "Name" field, for the keyboard and clipboard checks.
    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn name_mut(&mut self) -> &mut String {
        &mut self.name
    }

    pub fn last_event(&self) -> &'static str {
        self.last_event
    }

    pub fn set_last_event(&mut self, name: &'static str) {
        self.last_event = name;
    }
}

impl Default for DemoState {
    fn default() -> Self {
        Self::new()
    }
}

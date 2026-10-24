use demo_ffi::structs::{DemoConfig, DemoState};
use levi_ui::tokens::{ColorToken, color};

#[test]
fn a_bad_accent_text_falls_back_to_the_accent() {
    let mut config = DemoConfig::default();
    config.card.accent = "not a colour".to_string();
    let mut state = DemoState::new();

    state.apply_config(config);

    assert_eq!(state.accent(), <color::Accent as ColorToken>::VALUE);
}

#[test]
fn a_hex_accent_text_becomes_the_card_accent() {
    let mut config = DemoConfig::default();
    config.card.accent = "#102030".to_string();
    let mut state = DemoState::new();

    state.apply_config(config);

    assert_eq!(state.accent(), egui::Color32::from_rgb(0x10, 0x20, 0x30));
}

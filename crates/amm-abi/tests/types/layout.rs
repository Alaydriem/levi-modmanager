use std::mem::{align_of, offset_of, size_of};

use amm_abi::types::{
    AmmApi, AmmCursorShape, AmmDrawCmd, AmmEvent, AmmEventKind, AmmSamplerKind, AmmTextureDelta, AmmUiEvent,
    AmmUiEventKind, AmmVertex,
};
use amm_core::structs::frame::{CursorShape, UiEvent, UiEventKind};
use amm_core::structs::mesh::{DrawCmd, SamplerKind, TextureDelta, Vertex};

#[test]
fn api_table_field_offsets_are_stable() {
    assert_eq!(offset_of!(AmmApi, size), 0);
    assert_eq!(offset_of!(AmmApi, abi_major), 4);
    assert_eq!(offset_of!(AmmApi, abi_minor), 8);
    assert_eq!(offset_of!(AmmApi, register), 16);
    assert_eq!(offset_of!(AmmApi, log), 16 + 10 * 8);
    assert_eq!(offset_of!(AmmApi, request_screenshot), 16 + 11 * 8);
    assert_eq!(offset_of!(AmmApi, texture_budget), 16 + 13 * 8);
    assert_eq!(offset_of!(AmmApi, request_screenshot_layers), 16 + 14 * 8);
    assert_eq!(offset_of!(AmmApi, register_menu_entry), 16 + 15 * 8);
    assert_eq!(offset_of!(AmmApi, locale), 16 + 16 * 8);
    assert_eq!(offset_of!(AmmApi, set_icon), 16 + 17 * 8);
    assert_eq!(offset_of!(AmmApi, game_picture), 16 + 18 * 8);
    assert_eq!(offset_of!(AmmApi, set_game_viewport), 16 + 19 * 8);
    assert_eq!(size_of::<AmmApi>(), 16 + 20 * 8);
}

#[test]
fn later_event_kinds_keep_their_wire_values() {
    // a mod built for an older minor version decodes these bytes; a new kind only goes at the end
    assert_eq!(AmmEventKind::MenuEntryClicked as u8, 16);
    assert_eq!(AmmEventKind::LocaleChanged as u8, 17);
}

#[test]
fn event_has_no_padding_surprises() {
    assert_eq!(size_of::<AmmEvent>(), 16);
    assert_eq!(offset_of!(AmmEvent, code), 4);
}

#[test]
fn abi_vertex_matches_core_vertex_layout() {
    assert_eq!(size_of::<AmmVertex>(), size_of::<Vertex>());
    assert_eq!(align_of::<AmmVertex>(), align_of::<Vertex>());
}

#[test]
fn abi_draw_cmd_matches_core_draw_cmd_layout() {
    assert_eq!(size_of::<AmmDrawCmd>(), size_of::<DrawCmd>());
    assert_eq!(align_of::<AmmDrawCmd>(), align_of::<DrawCmd>());
    assert_eq!(offset_of!(AmmDrawCmd, clip), offset_of!(DrawCmd, clip));
    assert_eq!(offset_of!(AmmDrawCmd, sampler), offset_of!(DrawCmd, sampler));
}

#[test]
fn abi_texture_delta_matches_core_texture_delta_layout() {
    assert_eq!(size_of::<AmmTextureDelta>(), size_of::<TextureDelta>());
    assert_eq!(align_of::<AmmTextureDelta>(), align_of::<TextureDelta>());
    assert_eq!(offset_of!(AmmTextureDelta, byte_len), offset_of!(TextureDelta, byte_len));
}

#[test]
fn abi_ui_event_matches_core_ui_event_layout() {
    assert_eq!(size_of::<AmmUiEvent>(), size_of::<UiEvent>());
    assert_eq!(align_of::<AmmUiEvent>(), align_of::<UiEvent>());
    assert_eq!(offset_of!(AmmUiEvent, ch), offset_of!(UiEvent, ch));
    assert_eq!(offset_of!(AmmUiEvent, down), offset_of!(UiEvent, down));
}

#[test]
fn abi_draw_cmd_and_ui_event_field_offsets_match_core() {
    assert_eq!(offset_of!(AmmDrawCmd, shader), offset_of!(DrawCmd, shader));
    assert_eq!(offset_of!(AmmDrawCmd, texture), offset_of!(DrawCmd, texture));
    assert_eq!(offset_of!(AmmUiEvent, key), offset_of!(UiEvent, key));
    assert_eq!(offset_of!(AmmUiEvent, modifiers), offset_of!(UiEvent, modifiers));
}

#[test]
fn enum_values_match_their_mirrors() {
    assert_eq!(SamplerKind::Linear as u8, AmmSamplerKind::Linear as u8);
    assert_eq!(SamplerKind::Nearest as u8, AmmSamplerKind::Nearest as u8);

    assert_eq!(UiEventKind::PointerMove as u8, AmmUiEventKind::PointerMove as u8);
    assert_eq!(UiEventKind::PointerButton as u8, AmmUiEventKind::PointerButton as u8);
    assert_eq!(UiEventKind::Wheel as u8, AmmUiEventKind::Wheel as u8);
    assert_eq!(UiEventKind::Key as u8, AmmUiEventKind::Key as u8);
    assert_eq!(UiEventKind::Char as u8, AmmUiEventKind::Char as u8);
    assert_eq!(UiEventKind::FocusLost as u8, AmmUiEventKind::FocusLost as u8);

    assert_eq!(CursorShape::Default as u8, AmmCursorShape::Default as u8);
    assert_eq!(CursorShape::Arrow as u8, AmmCursorShape::Arrow as u8);
    assert_eq!(CursorShape::Hand as u8, AmmCursorShape::Hand as u8);
    assert_eq!(CursorShape::Text as u8, AmmCursorShape::Text as u8);
}

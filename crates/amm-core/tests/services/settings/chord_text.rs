use amm_core::services::settings::{ChordText, KeyNames};
use amm_core::structs::descriptor::Chord;

#[test]
fn chord_text_parses_modifiers_in_any_order_and_formats_canonically() {
    let expected = Chord { key: 0x4D, ctrl: true, shift: true, alt: false };

    assert_eq!(ChordText::parse("Ctrl+Shift+M"), Some(expected));
    assert_eq!(ChordText::parse("shift+CTRL+m"), Some(expected));
    assert_eq!(ChordText::format(expected), "Ctrl+Shift+M");

    let all = Chord { key: 0x70, ctrl: true, shift: true, alt: true };
    assert_eq!(ChordText::format(all), "Ctrl+Shift+Alt+F1");
    assert_eq!(ChordText::parse("Alt+Shift+Ctrl+F1"), Some(all));
    assert_eq!(ChordText::parse("Alt+F1+Ctrl"), None);
}

#[test]
fn keys_name_each_cap_in_canonical_order() {
    let chord = Chord { key: 0x79, ctrl: true, shift: true, alt: false };
    let unnamed = Chord { key: 0xE9, ctrl: false, shift: false, alt: true };

    assert_eq!(ChordText::keys(chord), ["Ctrl", "Shift", "F10"]);
    assert_eq!(ChordText::keys(unnamed), ["Alt", "VK_0xE9"]);
}

#[test]
fn modifier_alone_is_a_chord() {
    let ctrl = ChordText::parse("Ctrl");

    assert_eq!(ctrl, Some(Chord { key: 0x11, ctrl: false, shift: false, alt: false }));
    assert_eq!(ctrl.map(ChordText::format), Some("Ctrl".to_string()));
}

#[test]
fn unnamed_vk_round_trips_as_hex_name() {
    let chord = Chord { key: 0xE9, ctrl: false, shift: false, alt: true };

    assert_eq!(KeyNames::name(0xE9), None);
    assert_eq!(ChordText::format(chord), "Alt+VK_0xE9");
    assert_eq!(ChordText::parse("Alt+VK_0xE9"), Some(chord));
    assert_eq!(ChordText::parse("alt+vk_0xe9"), Some(chord));
}

#[test]
fn unknown_key_name_does_not_parse() {
    assert_eq!(KeyNames::vk("Banana"), None);
    assert_eq!(ChordText::parse("Ctrl+Banana"), None);
    assert_eq!(ChordText::parse(""), None);
    assert_eq!(ChordText::parse("Ctrl+"), None);
    assert_eq!(ChordText::parse("Ctrl+Ctrl+M"), None);
    assert_eq!(ChordText::parse("VK_0x"), None);
    assert_eq!(ChordText::parse("VK_0xZZ"), None);
}

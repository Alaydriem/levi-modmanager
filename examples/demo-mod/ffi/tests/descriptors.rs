use amm_core::services::descriptor::DescriptorChecker;
use amm_core::structs::descriptor::{Capability, Chord, ClaimMode, KeyScope, KeybindDecl};

const DEMO: &str = include_str!("../descriptor.hcl");
const SECOND: &str = include_str!("../descriptor_b.hcl");

fn checker() -> DescriptorChecker {
    DescriptorChecker::new(128, 256)
}

#[test]
fn demo_descriptor_passes_the_manager_checker() {
    let descriptor = checker().parse(DEMO).expect("the demo descriptor parses and checks");

    assert_eq!(descriptor.id, "alaydriem-demo");
    let claimed: Vec<_> = descriptor.claims.iter().map(|claim| (claim.capability, claim.mode)).collect();
    for capability in [Capability::Camera, Capability::Input, Capability::Overlay, Capability::Hud] {
        assert!(claimed.contains(&(capability, ClaimMode::Shared)), "{capability:?} is claimed shared");
    }
    let blocking: Vec<_> =
        descriptor.claims.iter().filter(|claim| claim.block_player_turn).map(|claim| claim.capability).collect();
    assert_eq!(blocking, [Capability::Input]);
    let actions: Vec<_> =
        descriptor.keybinds.iter().map(|keybind| (keybind.action.as_str(), keybind.capability)).collect();
    assert_eq!(
        actions,
        [
            ("toggle_card", Capability::Hud),
            ("take_camera", Capability::Camera),
            ("open_window", Capability::Overlay),
            ("screenshot", Capability::Hud),
            ("screenshot_layers", Capability::Hud),
        ]
    );
    assert!(descriptor.keybinds.iter().all(|keybind| keybind.scope == KeyScope::Global));
    let chords: Vec<_> = descriptor.keybinds.iter().map(|keybind| keybind.default_chord).collect();
    assert_eq!(chords, [alt(0x31), alt(0x32), alt(0x33), alt(0x34), alt(0x35)]);
}

fn alt(key: u32) -> Chord {
    Chord { key, ctrl: false, shift: false, alt: true }
}

#[test]
fn second_descriptor_claims_exclusive_camera_and_the_demo_card_chord() {
    let descriptor = checker().parse(SECOND).expect("the second descriptor parses and checks");
    let demo = checker().parse(DEMO).expect("the demo descriptor parses and checks");

    assert_eq!(descriptor.id, "alaydriem-demo-b");
    assert!(
        descriptor
            .claims
            .iter()
            .any(|claim| claim.capability == Capability::Camera && claim.mode == ClaimMode::Exclusive)
    );
    let alt_1 = alt(0x31);
    let global_on = |keybinds: &[KeybindDecl]| {
        keybinds.iter().any(|keybind| keybind.default_chord == alt_1 && keybind.scope == KeyScope::Global)
    };
    assert!(global_on(&descriptor.keybinds));
    assert!(global_on(&demo.keybinds));
}

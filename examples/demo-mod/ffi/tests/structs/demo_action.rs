use amm_core::services::descriptor::DescriptorChecker;
use demo_ffi::structs::DemoAction;

#[test]
fn action_indexes_follow_the_descriptor_keybind_order() {
    let descriptor = DescriptorChecker::new(128, 256).parse(include_str!("../../descriptor.hcl")).expect("descriptor");

    for (index, keybind) in descriptor.keybinds.iter().enumerate() {
        let action = DemoAction::from_index(index as u16);
        assert_eq!(action.map(DemoAction::name), Some(keybind.action.as_str()));
    }
}

#[test]
fn an_index_past_the_keybinds_is_no_action() {
    assert_eq!(DemoAction::from_index(5), None);
}

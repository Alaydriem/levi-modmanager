use amm_abi::types::AmmStatus;
use amm_client::test_support::FakeTable;
use amm_core::structs::descriptor::Capability;
use amm_core::structs::event::{KeybindPhase, ModEvent};
use amm_core::structs::lease::AcquireOutcome;
use amm_core::structs::registry::ModIndex;
use demo_ffi::services::DemoMod;
use tempfile::TempDir;

const TOGGLE_CARD_ACTION: u16 = 0;
const SCREENSHOT_ACTION: u16 = 3;
const SCREENSHOT_LAYERS_ACTION: u16 = 4;

fn press_screenshot() {
    FakeTable::push_event(ModEvent::Keybind { action: SCREENSHOT_ACTION, phase: KeybindPhase::Pressed });
}

fn demo(dir: &TempDir) -> DemoMod {
    match DemoMod::new(FakeTable::table(), dir.path()) {
        Ok(demo) => demo,
        Err(error) => panic!("demo not registered: {error}"),
    }
}

fn requested() -> Vec<String> {
    FakeTable::with(|state| state.screenshot_requests.clone())
}

#[test]
fn the_key_requests_a_numbered_png_in_the_shots_folder_and_creates_the_folder() {
    let dir = TempDir::new().expect("temp dir");
    let mut demo = demo(&dir);
    FakeTable::set_screenshot_answer(AmmStatus::Ok, 7);

    press_screenshot();
    demo.tick();
    FakeTable::push_event(ModEvent::ScreenshotWritten { sequence: 7, ok: true });
    demo.tick();
    press_screenshot();
    FakeTable::set_screenshot_answer(AmmStatus::Ok, 8);
    demo.tick();

    let shots = dir.path().join("shots");
    assert!(shots.is_dir(), "the demo creates the shots folder");
    assert_eq!(
        requested(),
        vec![
            shots.join("demo-1.png").to_str().expect("utf-8").to_owned(),
            shots.join("demo-2.png").to_str().expect("utf-8").to_owned(),
        ]
    );
}

#[test]
fn the_layers_key_requests_a_layers_png_with_its_own_numbering_and_the_layers_choice() {
    let dir = TempDir::new().expect("temp dir");
    let mut demo = demo(&dir);
    FakeTable::set_screenshot_answer(AmmStatus::Ok, 7);

    press_screenshot();
    demo.tick();
    FakeTable::push_event(ModEvent::ScreenshotWritten { sequence: 7, ok: true });
    demo.tick();
    FakeTable::set_screenshot_answer(AmmStatus::Ok, 8);
    FakeTable::push_event(ModEvent::Keybind { action: SCREENSHOT_LAYERS_ACTION, phase: KeybindPhase::Pressed });
    demo.tick();

    let shots = dir.path().join("shots");
    assert_eq!(
        requested(),
        vec![
            shots.join("demo-1.png").to_str().expect("utf-8").to_owned(),
            shots.join("demo-layers-1.png").to_str().expect("utf-8").to_owned(),
        ]
    );
    assert_eq!(FakeTable::with(|state| state.screenshot_layers.clone()), vec![0, 1]);
    assert_eq!(demo.screenshot_line(), "Taking demo-layers-1.png");

    FakeTable::push_event(ModEvent::ScreenshotWritten { sequence: 8, ok: true });
    demo.tick();

    assert_eq!(demo.screenshot_line(), "Saved demo-layers-1.png");
}

#[test]
fn a_hidden_card_takes_the_hud_lease_for_the_request_and_gives_it_back_when_written() {
    let dir = TempDir::new().expect("temp dir");
    let mut demo = demo(&dir);
    FakeTable::set_screenshot_answer(AmmStatus::Ok, 3);

    press_screenshot();
    demo.tick();
    assert_eq!(FakeTable::with(|state| state.acquired.clone()), vec![Capability::Hud]);
    assert!(FakeTable::with(|state| state.released.is_empty()), "the lease stays until the file is written");

    FakeTable::push_event(ModEvent::ScreenshotTaken { sequence: 3, ok: true });
    demo.tick();
    assert!(FakeTable::with(|state| state.released.is_empty()));

    FakeTable::push_event(ModEvent::ScreenshotWritten { sequence: 3, ok: true });
    demo.tick();
    assert_eq!(FakeTable::with(|state| state.released.clone()), vec![Capability::Hud]);
}

#[test]
fn a_shown_card_keeps_its_own_hud_lease() {
    let dir = TempDir::new().expect("temp dir");
    let mut demo = demo(&dir);
    FakeTable::push_event(ModEvent::Enabled);
    demo.tick();
    FakeTable::set_screenshot_answer(AmmStatus::Ok, 3);

    press_screenshot();
    demo.tick();
    FakeTable::push_event(ModEvent::ScreenshotWritten { sequence: 3, ok: true });
    demo.tick();

    assert_eq!(FakeTable::with(|state| state.acquired.clone()), vec![Capability::Hud]);
    assert!(FakeTable::with(|state| state.released.is_empty()));
}

#[test]
fn the_events_set_the_card_text() {
    let dir = TempDir::new().expect("temp dir");
    let mut demo = demo(&dir);
    FakeTable::set_screenshot_answer(AmmStatus::Ok, 5);

    press_screenshot();
    demo.tick();
    FakeTable::push_event(ModEvent::ScreenshotWritten { sequence: 5, ok: true });
    demo.tick();
    assert_eq!(demo.screenshot_line(), "Saved demo-1.png");

    FakeTable::set_screenshot_answer(AmmStatus::Ok, 6);
    press_screenshot();
    demo.tick();
    FakeTable::push_event(ModEvent::ScreenshotTaken { sequence: 6, ok: false });
    demo.tick();
    assert_eq!(demo.screenshot_line(), "Screenshot failed");
}

#[test]
fn a_failed_write_shows_failed_and_gives_the_lease_back() {
    let dir = TempDir::new().expect("temp dir");
    let mut demo = demo(&dir);
    FakeTable::set_screenshot_answer(AmmStatus::Ok, 5);

    press_screenshot();
    demo.tick();
    FakeTable::push_event(ModEvent::ScreenshotWritten { sequence: 5, ok: false });
    demo.tick();

    assert_eq!(demo.screenshot_line(), "Screenshot failed");
    assert_eq!(FakeTable::with(|state| state.released.clone()), vec![Capability::Hud]);
}

#[test]
fn a_refused_request_shows_failed_without_a_crash_and_gives_the_lease_back() {
    for status in [AmmStatus::Busy, AmmStatus::NotPermitted] {
        let dir = TempDir::new().expect("temp dir");
        let mut demo = demo(&dir);
        FakeTable::with(|state| state.released.clear());
        FakeTable::set_screenshot_answer(status, 0);

        press_screenshot();
        demo.tick();

        assert_eq!(demo.screenshot_line(), "Screenshot failed", "{status:?}");
        assert_eq!(requested().len(), 1);
        assert_eq!(FakeTable::with(|state| state.released.clone()), vec![Capability::Hud], "{status:?}");
        FakeTable::with(|state| state.screenshot_requests.clear());
    }
}

#[test]
fn a_denied_hud_lease_shows_failed_and_sends_no_request() {
    let dir = TempDir::new().expect("temp dir");
    let mut demo = demo(&dir);
    FakeTable::set_acquire_outcome(Capability::Hud, AcquireOutcome::Denied { holder: ModIndex(2) });

    press_screenshot();
    demo.tick();

    assert_eq!(demo.screenshot_line(), "Screenshot failed");
    assert!(requested().is_empty());
}

#[test]
fn an_event_for_another_sequence_is_ignored() {
    let dir = TempDir::new().expect("temp dir");
    let mut demo = demo(&dir);
    FakeTable::set_screenshot_answer(AmmStatus::Ok, 5);

    press_screenshot();
    demo.tick();
    FakeTable::push_event(ModEvent::ScreenshotWritten { sequence: 99, ok: true });
    demo.tick();

    assert_eq!(demo.screenshot_line(), "Taking demo-1.png");
    assert!(FakeTable::with(|state| state.released.is_empty()));
}

#[test]
fn a_card_shown_while_a_temporary_hud_shot_is_in_flight_keeps_its_hud_lease() {
    let dir = TempDir::new().expect("temp dir");
    let mut demo = demo(&dir);
    FakeTable::set_screenshot_answer(AmmStatus::Ok, 4);

    press_screenshot();
    demo.tick();
    FakeTable::push_event(ModEvent::Keybind { action: TOGGLE_CARD_ACTION, phase: KeybindPhase::Pressed });
    demo.tick();
    FakeTable::push_event(ModEvent::ScreenshotWritten { sequence: 4, ok: true });
    demo.tick();

    assert_eq!(FakeTable::with(|state| state.acquired.clone()), vec![Capability::Hud, Capability::Hud]);
    assert!(FakeTable::with(|state| state.released.is_empty()), "the shown card keeps the Hud lease");
    assert_eq!(demo.screenshot_line(), "Saved demo-1.png");
}

#[test]
fn the_demo_reads_the_language_at_start_and_on_a_change() {
    let dir = TempDir::new().expect("temp dir");
    let mut demo = demo(&dir);
    let at_start = FakeTable::with(|state| state.locale_reads);

    FakeTable::with(|state| state.locale = "en_XA");
    FakeTable::push_event(ModEvent::LocaleChanged);
    demo.tick();

    assert_eq!(at_start, 1);
    assert_eq!(FakeTable::with(|state| state.locale_reads), 2);
}

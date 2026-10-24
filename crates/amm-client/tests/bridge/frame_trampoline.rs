use amm_abi::types::{AmmLogLevel, AmmStatus};

use crate::support::{FakeTable, PanickingHandler, PayloadPanickingHandler};

#[test]
fn panicking_frame_handler_returns_panic_status_and_does_not_unwind() {
    let link = FakeTable::link(PanickingHandler);

    let first = FakeTable::run_frame();
    let second = FakeTable::run_frame();

    assert_eq!(first, AmmStatus::Panic as i32);
    assert_eq!(second, AmmStatus::Panic as i32);
    let errors =
        FakeTable::with(|state| state.logs.iter().filter(|call| call.level == AmmLogLevel::Error as u8).count());
    assert_eq!(errors, 1, "a panicking handler is logged once");
    drop(link);
}

#[test]
fn a_null_input_is_a_null_pointer_status() {
    let link = FakeTable::link(PanickingHandler);
    let callbacks = FakeTable::with(|state| state.callbacks);
    let Some(callbacks) = callbacks else { panic!("nothing registered") };
    let Some(frame) = callbacks.frame else { panic!("no frame callback") };
    let mut out = FakeTable::empty_output();

    // SAFETY: the user pointer is the one the link registered and out lives for the call
    let status = unsafe { frame(callbacks.user, std::ptr::null(), &mut out) };

    assert_eq!(status, AmmStatus::NullPointer as i32);
    drop(link);
}

#[test]
fn a_panic_payload_that_panics_when_dropped_still_returns_panic_status() {
    let link = FakeTable::link(PayloadPanickingHandler);

    let first = FakeTable::run_frame();
    let second = FakeTable::run_frame();

    assert_eq!(first, AmmStatus::Panic as i32);
    assert_eq!(second, AmmStatus::Panic as i32);
    drop(link);
}

use std::path::Path;

use amm_core::services::PanicGuard;

use super::RawText;
use crate::services::DemoSlot;
use crate::structs::DemoStatus;

/// Registers the demo with the mod manager. `config_dir` is the demo's own LeviLamina config folder as UTF-8
/// text. Returns `DEMO_STATUS_OK` on success; `DEMO_STATUS_MANAGER_MISSING` when the mod manager is not loaded,
/// in which case the demo stays inert and later calls do nothing. Call it from the mod's enable, never under the OS
/// loader lock: the manager starts a log writer thread for each registration.
///
/// # Safety
///
/// `config_dir` is null or holds `len` readable bytes for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn demo_create(config_dir: *const u8, len: usize) -> DemoStatus {
    PanicGuard::run(
        || {
            // SAFETY: the shim passes the UTF-8 bytes of a live std::string for the call
            let Some(text) = (unsafe { RawText::read(config_dir, len) }) else {
                return DemoStatus::InvalidArgument;
            };
            DemoSlot::create(Path::new(text))
        },
        |payload| curia::error!("demo_create panicked", { payload: payload }),
    )
    .unwrap_or(DemoStatus::Panic)
}

/// Handles the demo's queued events. Call it once per client update on the game thread.
#[unsafe(no_mangle)]
pub extern "C" fn demo_tick() {
    PanicGuard::run(DemoSlot::tick, |payload| curia::error!("demo_tick panicked", { payload: payload }));
}

/// Unregisters the demo. Call it after the update hook is gone. Safe to call when nothing is created.
#[unsafe(no_mangle)]
pub extern "C" fn demo_destroy() {
    PanicGuard::run(DemoSlot::destroy, |payload| curia::error!("demo_destroy panicked", { payload: payload }));
}

/// Sends one shim log line to the mod manager's log. `level` is an `AmmLogLevel` value, `fields_json` a JSON
/// object. Nothing is sent while the demo is not registered.
///
/// # Safety
///
/// Each text pointer is null or holds its length in readable bytes for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn demo_log(
    level: u8,
    message: *const u8,
    message_len: usize,
    fields_json: *const u8,
    fields_len: usize,
    file: *const u8,
    file_len: usize,
    line: u32,
) {
    let texts = || {
        // SAFETY: the shim passes the bytes of live string views for the call
        let texts = unsafe {
            (RawText::read(message, message_len), RawText::read(fields_json, fields_len), RawText::read(file, file_len))
        };
        if let (Some(message), Some(fields), Some(file)) = texts {
            DemoSlot::log(level, message, fields, file, line);
        }
    };
    PanicGuard::run(texts, |payload| curia::error!("demo_log panicked", { payload: payload }));
}

use std::path::Path;
use std::sync::{Mutex, MutexGuard, PoisonError};

use amm_abi::types::AmmLogLevel;
use amm_client::link::ApiTable;
use curia::{Fields, Level, LogEvent, Logger};

use super::DemoMod;
use crate::structs::DemoStatus;

/// The one demo mod of this DLL. The shim creates it at enable, ticks it from the client update and destroys it
/// at disable, all on the game thread.
pub struct DemoSlot;

impl DemoSlot {
    const LOG_TARGET: &'static str = "alaydriem_demo::shim";

    fn slot() -> MutexGuard<'static, Option<DemoMod>> {
        static SLOT: Mutex<Option<DemoMod>> = Mutex::new(None);
        SLOT.lock().unwrap_or_else(PoisonError::into_inner)
    }

    /// Finds the manager and registers the demo. A manager that is not loaded is `ManagerMissing` and leaves the
    /// slot empty, so every later call does nothing.
    pub fn create(config_dir: &Path) -> DemoStatus {
        let mut slot = Self::slot();
        if slot.is_some() {
            return DemoStatus::AlreadyCreated;
        }
        let table = match ApiTable::resolve() {
            Ok(table) => table,
            Err(error) => return DemoStatus::from(&error),
        };
        match DemoMod::new(table, config_dir) {
            Ok(demo) => {
                *slot = Some(demo);
                DemoStatus::Ok
            }
            Err(error) => DemoStatus::from(&error),
        }
    }

    pub fn tick() {
        if let Some(demo) = Self::slot().as_mut() {
            demo.tick();
        }
    }

    /// Unregisters the demo, and the second mod with it. Safe to call when nothing is created.
    pub fn destroy() {
        let taken = Self::slot().take();
        drop(taken);
    }

    /// Sends one shim log line up through the demo's registration. A line logged while the demo is not
    /// registered is dropped, because Curia has no sink yet.
    pub fn log(level: u8, message: &str, fields_json: &str, file: &str, line: u32) {
        let Some(level) = Self::level(level) else { return };
        let fields = match serde_json::from_str::<serde_json::Map<String, serde_json::Value>>(fields_json) {
            Ok(map) => Fields::from_map(map),
            Err(_) => Fields::new(),
        };
        Logger::emit(LogEvent {
            level,
            target: Self::LOG_TARGET.to_string(),
            message: message.to_string(),
            fields,
            timestamp: chrono::Utc::now(),
            file: Some(file.to_string()),
            line: Some(line),
        });
    }

    fn level(code: u8) -> Option<Level> {
        match code {
            code if code == AmmLogLevel::Error as u8 => Some(Level::Error),
            code if code == AmmLogLevel::Warn as u8 => Some(Level::Warn),
            code if code == AmmLogLevel::Info as u8 => Some(Level::Info),
            code if code == AmmLogLevel::Debug as u8 => Some(Level::Debug),
            code if code == AmmLogLevel::Trace as u8 => Some(Level::Trace),
            _ => None,
        }
    }
}

use amm_core::structs::event::ModEvent;

/// The fixed text a log line uses for an event kind.
pub struct EventName;

impl EventName {
    pub fn of(event: &ModEvent) -> &'static str {
        match event {
            ModEvent::Enabled => "enabled",
            ModEvent::Disabled => "disabled",
            ModEvent::Granted(_) => "granted",
            ModEvent::Revoked { .. } => "revoked",
            ModEvent::Suspended { .. } => "suspended",
            ModEvent::Resumed(_) => "resumed",
            ModEvent::CapabilityAvailable(_) => "capability_available",
            ModEvent::Keybind { .. } => "keybind",
            ModEvent::SettingsChanged { .. } => "settings_changed",
            ModEvent::WorldReloadApply => "world_reload_apply",
            ModEvent::Key { .. } => "key",
            ModEvent::PointerButton { .. } => "pointer_button",
            ModEvent::PointerDelta { .. } => "pointer_delta",
            ModEvent::Wheel { .. } => "wheel",
            ModEvent::ScreenshotTaken { .. } => "screenshot_taken",
            ModEvent::ScreenshotWritten { .. } => "screenshot_written",
            ModEvent::MenuEntryClicked { .. } => "menu_entry_clicked",
            ModEvent::LocaleChanged => "locale_changed",
        }
    }
}

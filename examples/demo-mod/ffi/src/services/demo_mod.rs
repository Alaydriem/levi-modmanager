use std::path::{Path, PathBuf};
use std::sync::Arc;

use amm_client::link::{ApiTable, EventDrain, ManagerLink};
use amm_core::errors::LinkError;
use amm_core::structs::capture::ScreenshotLayers;
use amm_core::structs::descriptor::Capability;
use amm_core::structs::event::{KeybindPhase, ModEvent};
use amm_core::structs::lease::AcquireOutcome;
use levi_i18n::message::Message;
use levi_i18n::service::{LocaleSetting, LocalizationService};
use levi_ui::LeviUiMessage;
use levi_ui::text::LeviUiPacks;

use super::{DemoUi, SecondMod};
use crate::structs::{DemoAction, DemoCell, DemoConfig, EventName, PendingShot};

/// The demo mod: one registration with the manager, the state its card and window draw from, and the optional
/// second registration. The shim calls `tick` once per client update on the game thread.
pub struct DemoMod {
    link: ManagerLink,
    cell: DemoCell,
    second: Option<SecondMod>,
    hud_held: bool,
    camera_held: bool,
    overlay_held: bool,
    input_held: bool,
    cursor_on: bool,
    // the screenshot in flight, if any; the manager allows one per mod
    pending_shot: Option<PendingShot>,
    shots_dir: PathBuf,
    // screenshots requested so far this session, the number in the next file name
    shot_count: u32,
    // screenshots with mod layers requested so far this session, the number in the next file name
    layer_shot_count: u32,
    // events of one poll, at most one drain buffer
    pending: Vec<ModEvent>,
}

impl DemoMod {
    const DESCRIPTOR: &'static str = include_str!("../../descriptor.hcl");
    const ICON: &'static [u8] = include_bytes!("../../icon.png");
    const SECOND_DIR: &'static str = "second";
    const SHOTS_DIR: &'static str = "shots";
    const SHOT_FAILED: &'static str = "Screenshot failed";

    /// Registers the demo from its descriptor. `config_dir` is the mod's own LeviLamina config folder. When the
    /// `debug_second_mod` setting is on, `alaydriem-demo-b` is registered too, with its own config folder inside
    /// this one.
    pub fn new(table: ApiTable, config_dir: &Path) -> Result<Self, LinkError> {
        if let Err(error) = std::fs::create_dir_all(config_dir) {
            curia::warn!("config folder not created", { error: error.to_string() });
        }
        let cell = DemoCell::new();
        let link = ManagerLink::new(table, Self::DESCRIPTOR, config_dir, DemoUi::new(cell.clone()))?;
        if let Err(error) = link.set_icon(Self::ICON) {
            curia::warn!("icon not set", { error: error.to_string() });
        }
        let mut demo = Self {
            link,
            cell,
            second: None,
            hud_held: false,
            camera_held: false,
            overlay_held: false,
            input_held: false,
            cursor_on: false,
            pending_shot: None,
            shots_dir: config_dir.join(Self::SHOTS_DIR),
            shot_count: 0,
            layer_shot_count: 0,
            pending: Vec::with_capacity(EventDrain::CAPACITY),
        };
        demo.reload_config();
        demo.apply_locale();
        if demo.cell.with(|state| state.config().debug_second_mod) {
            demo.second = Self::register_second(table, config_dir);
        }
        curia::info!("demo registered", { second_mod: demo.second.is_some() });
        Ok(demo)
    }

    /// Handles the events queued for the mod, then puts the cursor and the leases in line with the window.
    pub fn tick(&mut self) {
        self.pending.clear();
        self.pending.extend(self.link.events());
        for position in 0..self.pending.len() {
            let event = self.pending[position];
            let name = EventName::of(&event);
            curia::debug!("event", { kind: name });
            self.cell.with(|state| state.set_last_event(name));
            self.handle(event);
        }
        self.sync_window();
        if let Some(second) = self.second.as_mut() {
            second.tick();
        }
    }

    /// The screenshot line the card shows, empty before the first screenshot key press.
    pub fn screenshot_line(&self) -> String {
        self.cell.with(|state| state.screenshot_line().to_string())
    }

    fn register_second(table: ApiTable, config_dir: &Path) -> Option<SecondMod> {
        let second_dir = config_dir.join(Self::SECOND_DIR);
        if let Err(error) = std::fs::create_dir_all(&second_dir) {
            curia::warn!("second mod config folder not created", { error: error.to_string() });
            return None;
        }
        match SecondMod::new(table, &second_dir) {
            Ok(second) => Some(second),
            Err(error) => {
                curia::warn!("second mod not registered", { error: error.to_string() });
                None
            }
        }
    }

    // The widgets' own words follow the manager's language. A manager older than ABI 1.5 has none: English.
    fn apply_locale(&self) {
        let Some(code) = self.link.locale() else {
            return;
        };
        let service = LocalizationService::new(vec![(LeviUiMessage::DOMAIN, LeviUiPacks::SOURCE)]);
        self.link.set_localizer(Arc::new(service.load(&LocaleSetting::from_code(&code))));
    }

    fn reload_config(&mut self) {
        match self.link.config::<DemoConfig>() {
            Ok(config) => self.cell.with(|state| state.apply_config(config)),
            Err(error) => curia::warn!("config not read, the last one stays", { error: error.to_string() }),
        }
    }

    fn handle(&mut self, event: ModEvent) {
        match event {
            ModEvent::Enabled => self.show_card(),
            ModEvent::Disabled => self.reset(),
            ModEvent::Revoked { capability, .. } => self.lose(capability, "revoked"),
            ModEvent::Suspended { capability, .. } => self.suspend(capability),
            ModEvent::Resumed(Capability::Camera) => self.set_camera_line("Camera: resumed".to_string()),
            ModEvent::CapabilityAvailable(Capability::Camera) => {
                self.set_camera_line("Camera: free again, press the key to retry".to_string());
            }
            ModEvent::Keybind { action, phase: KeybindPhase::Pressed } => {
                if let Some(action) = DemoAction::from_index(action) {
                    self.run(action);
                }
            }
            ModEvent::SettingsChanged { .. } => self.reload_config(),
            ModEvent::LocaleChanged => self.apply_locale(),
            ModEvent::ScreenshotTaken { sequence, ok: false } => self.finish_shot(sequence, false),
            ModEvent::ScreenshotWritten { sequence, ok } => self.finish_shot(sequence, ok),
            _ => {}
        }
    }

    fn run(&mut self, action: DemoAction) {
        match action {
            DemoAction::ToggleCard => self.toggle_card(),
            DemoAction::TakeCamera => self.toggle_camera(),
            DemoAction::OpenWindow => self.toggle_window(),
            DemoAction::Screenshot => self.take_screenshot(ScreenshotLayers::GameOnly),
            DemoAction::ScreenshotWithLayers => self.take_screenshot(ScreenshotLayers::WithModLayers),
        }
    }

    // A capture needs an active Hud or Overlay lease. A shown card already holds Hud. With the card hidden, the
    // demo takes Hud for the request and gives it back when the file is written or the capture fails.
    fn take_screenshot(&mut self, layers: ScreenshotLayers) {
        let shots = self.shots_dir.clone();
        if let Err(error) = std::fs::create_dir_all(&shots) {
            curia::warn!("shots folder not created", { error: error.to_string() });
            return self.show_shot_line(Self::SHOT_FAILED.to_string());
        }
        let temporary_hud = !self.hud_held && self.pending_shot.is_none();
        if temporary_hud && !self.acquire_hud_for_shot() {
            return self.show_shot_line(Self::SHOT_FAILED.to_string());
        }
        let number = match layers {
            ScreenshotLayers::GameOnly => self.shot_count + 1,
            ScreenshotLayers::WithModLayers => self.layer_shot_count + 1,
        };
        let name = PendingShot::file_name(layers, number);
        match self.link.request_screenshot_with(&shots.join(&name), layers) {
            Ok(sequence) => {
                match layers {
                    ScreenshotLayers::GameOnly => self.shot_count = number,
                    ScreenshotLayers::WithModLayers => self.layer_shot_count = number,
                }
                self.pending_shot = Some(PendingShot { sequence, number, layers, temporary_hud });
                self.show_shot_line(format!("Taking {name}"));
            }
            Err(error) => {
                curia::warn!("screenshot refused", { error: error.to_string() });
                if temporary_hud {
                    self.link.release(Capability::Hud);
                }
                self.show_shot_line(Self::SHOT_FAILED.to_string());
            }
        }
    }

    fn acquire_hud_for_shot(&mut self) -> bool {
        match self.link.acquire(Capability::Hud) {
            Ok(AcquireOutcome::Granted | AcquireOutcome::Preempted) => true,
            Ok(AcquireOutcome::Denied { .. }) => false,
            Err(error) => {
                curia::warn!("hud request failed", { error: error.to_string() });
                false
            }
        }
    }

    // The result of the capture in flight. An event for another sequence is not ours.
    fn finish_shot(&mut self, sequence: u32, ok: bool) {
        let Some(shot) = self.pending_shot.filter(|shot| shot.sequence == sequence) else {
            return;
        };
        self.pending_shot = None;
        // a card shown while the shot was in flight now holds the same Hud lease
        if shot.temporary_hud && !self.hud_held {
            self.link.release(Capability::Hud);
        }
        let line = if ok {
            format!("Saved {}", PendingShot::file_name(shot.layers, shot.number))
        } else {
            Self::SHOT_FAILED.to_string()
        };
        self.show_shot_line(line);
    }

    fn show_shot_line(&mut self, line: String) {
        self.cell.with(|state| state.set_screenshot_line(line));
    }

    fn show_card(&mut self) {
        match self.link.acquire(Capability::Hud) {
            Ok(AcquireOutcome::Granted | AcquireOutcome::Preempted) => {
                self.hud_held = true;
                self.cell.with(|state| state.set_card_visible(true));
            }
            Ok(AcquireOutcome::Denied { .. }) => self.cell.with(|state| state.set_last_event("hud denied")),
            Err(error) => curia::warn!("hud request failed", { error: error.to_string() }),
        }
    }

    fn toggle_card(&mut self) {
        if self.hud_held {
            self.link.release(Capability::Hud);
            self.hud_held = false;
            self.cell.with(|state| state.set_card_visible(false));
        } else {
            self.show_card();
        }
    }

    fn toggle_camera(&mut self) {
        if self.camera_held {
            self.link.release(Capability::Camera);
            self.camera_held = false;
            self.set_camera_line("Camera: released".to_string());
            return;
        }
        match self.link.acquire(Capability::Camera) {
            Ok(AcquireOutcome::Granted) => {
                self.camera_held = true;
                self.set_camera_line("Camera: held".to_string());
            }
            Ok(AcquireOutcome::Preempted) => {
                self.camera_held = true;
                self.set_camera_line("Camera: held, taken from another mod".to_string());
            }
            Ok(AcquireOutcome::Denied { .. }) => {
                let holder = self.link.holder(Capability::Camera).unwrap_or_else(|| "another mod".to_string());
                self.set_camera_line(format!("Camera: denied, held by {holder}"));
            }
            Err(error) => curia::warn!("camera request failed", { error: error.to_string() }),
        }
    }

    fn toggle_window(&mut self) {
        if self.cell.with(|state| state.window_open()) {
            self.cell.with(|state| state.set_window_open(false));
            return;
        }
        match self.link.acquire(Capability::Overlay) {
            Ok(AcquireOutcome::Granted | AcquireOutcome::Preempted) => {
                self.overlay_held = true;
                self.input_held = matches!(
                    self.link.acquire(Capability::Input),
                    Ok(AcquireOutcome::Granted | AcquireOutcome::Preempted)
                );
                self.cell.with(|state| state.set_window_open(true));
            }
            Ok(AcquireOutcome::Denied { .. }) => self.cell.with(|state| state.set_last_event("overlay denied")),
            Err(error) => curia::warn!("overlay request failed", { error: error.to_string() }),
        }
    }

    // A window closed from its own button leaves the Overlay and Input leases and the cursor to be given back.
    fn sync_window(&mut self) {
        let open = self.cell.with(|state| state.window_open());
        if !open {
            if self.overlay_held {
                self.link.release(Capability::Overlay);
                self.overlay_held = false;
            }
            if self.input_held {
                self.link.release(Capability::Input);
                self.input_held = false;
            }
        }
        if open != self.cursor_on {
            match self.link.set_cursor_mode(open) {
                Ok(()) => self.cursor_on = open,
                Err(error) => curia::warn!("cursor mode not changed", { error: error.to_string() }),
            }
        }
    }

    fn lose(&mut self, capability: Capability, how: &'static str) {
        match capability {
            Capability::Hud => {
                self.hud_held = false;
                self.cell.with(|state| state.set_card_visible(false));
            }
            Capability::Camera => {
                self.camera_held = false;
                self.set_camera_line(format!("Camera: {how}"));
            }
            Capability::Overlay => {
                self.overlay_held = false;
                self.cell.with(|state| state.set_window_open(false));
            }
            Capability::Input => self.input_held = false,
            Capability::WorldDraw => {}
        }
    }

    fn suspend(&mut self, capability: Capability) {
        match capability {
            Capability::Camera => {
                let holder = self.link.holder(Capability::Camera).unwrap_or_else(|| "another mod".to_string());
                self.set_camera_line(format!("Camera: suspended by {holder}"));
            }
            Capability::Overlay => self.cell.with(|state| state.set_window_open(false)),
            _ => {}
        }
    }

    // The manager gave back every lease when the mod was disabled.
    fn reset(&mut self) {
        self.pending_shot = None;
        self.hud_held = false;
        self.camera_held = false;
        self.overlay_held = false;
        self.input_held = false;
        self.cell.with(|state| {
            state.set_card_visible(false);
            state.set_window_open(false);
            state.set_camera_line("Camera: not held".to_string());
        });
    }

    fn set_camera_line(&mut self, line: String) {
        self.cell.with(|state| state.set_camera_line(line));
    }
}

use std::path::Path;

use amm_client::link::{ApiTable, EventDrain, ManagerLink};
use amm_core::errors::LinkError;
use amm_core::structs::descriptor::Capability;
use amm_core::structs::event::{KeybindPhase, ModEvent};
use amm_core::structs::lease::AcquireOutcome;

use super::IdleUi;
use crate::structs::EventName;

/// The second registration, `alaydriem-demo-b`: it takes and gives back the camera with an exclusive claim, so
/// the demo mod is suspended while it holds the camera and a denied request shows a toast.
pub struct SecondMod {
    link: ManagerLink,
    camera_held: bool,
    // events of one poll, at most one drain buffer
    pending: Vec<ModEvent>,
}

impl SecondMod {
    const DESCRIPTOR: &'static str = include_str!("../../descriptor_b.hcl");
    const TOGGLE_CAMERA: u16 = 0;

    pub fn new(table: ApiTable, config_dir: &Path) -> Result<Self, LinkError> {
        let link = ManagerLink::new(table, Self::DESCRIPTOR, config_dir, IdleUi)?;
        Ok(Self { link, camera_held: false, pending: Vec::with_capacity(EventDrain::CAPACITY) })
    }

    pub fn tick(&mut self) {
        self.pending.clear();
        self.pending.extend(self.link.events());
        for position in 0..self.pending.len() {
            let event = self.pending[position];
            curia::debug!("event", { mod_id: "alaydriem-demo-b", kind: EventName::of(&event) });
            self.handle(event);
        }
    }

    fn handle(&mut self, event: ModEvent) {
        match event {
            ModEvent::Keybind { action: Self::TOGGLE_CAMERA, phase: KeybindPhase::Pressed } => self.toggle_camera(),
            ModEvent::Disabled | ModEvent::Revoked { capability: Capability::Camera, .. } => self.camera_held = false,
            _ => {}
        }
    }

    fn toggle_camera(&mut self) {
        if self.camera_held {
            self.link.release(Capability::Camera);
            self.camera_held = false;
            curia::info!("camera released", { mod_id: "alaydriem-demo-b" });
            return;
        }
        match self.link.acquire(Capability::Camera) {
            Ok(AcquireOutcome::Granted | AcquireOutcome::Preempted) => {
                self.camera_held = true;
                curia::info!("camera taken", { mod_id: "alaydriem-demo-b" });
            }
            Ok(AcquireOutcome::Denied { .. }) => curia::info!("camera denied", { mod_id: "alaydriem-demo-b" }),
            Err(error) => curia::warn!("camera request failed", { error: error.to_string() }),
        }
    }
}

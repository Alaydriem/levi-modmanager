use std::collections::{HashMap, VecDeque};

use amm_abi::types::{AmmCallbacks, AmmEvent, AmmHandle, AmmStatus};
use amm_core::structs::descriptor::Capability;
use amm_core::structs::lease::AcquireOutcome;
use amm_core::structs::manager::TextureGrant;

use super::{LoggedCall, SettingCall};

/// What the fake table recorded, and what it answers with. A test sets the answers before the call and reads
/// the records after it. The records grow with every call; a test makes few calls, so they have no bound.
#[derive(Debug)]
pub struct FakeState {
    pub register_status: AmmStatus,
    pub handle: AmmHandle,
    pub descriptor: Option<String>,
    pub config_dir: Option<String>,
    pub callbacks: Option<AmmCallbacks>,
    pub unregistered: Vec<AmmHandle>,
    pub unregister_status: AmmStatus,
    pub holder_id: Option<&'static str>,
    pub events: VecDeque<AmmEvent>,
    pub config: String,
    // The answer to `acquire` per capability; a capability not in the map is granted
    pub acquire_outcomes: HashMap<Capability, AcquireOutcome>,
    pub acquired: Vec<Capability>,
    pub released: Vec<Capability>,
    pub set_setting_status: AmmStatus,
    pub settings: Vec<SettingCall>,
    pub logs: Vec<LoggedCall>,
    // The status and sequence `request_screenshot` answers with
    pub screenshot_status: AmmStatus,
    pub screenshot_sequence: u32,
    // The path text of each `request_screenshot` call
    pub screenshot_requests: Vec<String>,
    // The `layers` byte of each `request_screenshot_layers` call, whose path is also in `screenshot_requests`
    pub screenshot_layers: Vec<u8>,
    // The answer to `held_by_other` per capability; a capability not in the map is not held by another mod
    pub held_by_other: HashMap<Capability, bool>,
    pub texture_grant: TextureGrant,
    // (label, mark, screens) of each `register_menu_entry` call, and the id it answers with
    pub menu_entries: Vec<(String, String, u8)>,
    pub menu_entry_id: u16,
    // The language code `locale` answers with; empty is English. `locale_reads` counts the calls
    pub locale: &'static str,
    pub locale_reads: u32,
    // The length in bytes of each `set_icon` call, and the status it answers with
    pub icon_calls: Vec<usize>,
    pub icon_status: AmmStatus,
    // the status and the size `game_picture` answers
    pub game_picture: (AmmStatus, u32, u32),
    // every rect `set_game_viewport` got, `None` for a clear
    pub game_viewports: Vec<Option<[f32; 4]>>,
}

impl FakeState {
    pub fn new() -> Self {
        Self {
            register_status: AmmStatus::Ok,
            handle: AmmHandle { slot: 3, generation: 7 },
            descriptor: None,
            config_dir: None,
            callbacks: None,
            unregistered: Vec::new(),
            unregister_status: AmmStatus::Ok,
            holder_id: None,
            events: VecDeque::new(),
            config: String::new(),
            acquire_outcomes: HashMap::new(),
            acquired: Vec::new(),
            released: Vec::new(),
            set_setting_status: AmmStatus::Ok,
            settings: Vec::new(),
            logs: Vec::new(),
            screenshot_status: AmmStatus::Ok,
            screenshot_sequence: 1,
            screenshot_requests: Vec::new(),
            screenshot_layers: Vec::new(),
            held_by_other: HashMap::new(),
            texture_grant: TextureGrant { bytes: 0, count: 0 },
            menu_entries: Vec::new(),
            menu_entry_id: 1,
            locale: "",
            locale_reads: 0,
            icon_calls: Vec::new(),
            icon_status: AmmStatus::Ok,
            game_picture: (AmmStatus::Ok, 0, 0),
            game_viewports: Vec::new(),
        }
    }

    /// True when a mod registered and gave neither a frame nor a textures-lost callback.
    pub fn registered_without_frame(&self) -> bool {
        self.callbacks.is_some_and(|callbacks| callbacks.frame.is_none() && callbacks.textures_lost.is_none())
    }
}

impl Default for FakeState {
    fn default() -> Self {
        Self::new()
    }
}

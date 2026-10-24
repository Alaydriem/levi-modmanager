use serde::Deserialize;

use super::CardConfig;

/// The demo's `config.hcl` as the mod reads it. Keys the demo does not read, such as the chords, are ignored.
#[derive(Debug, Clone, Default, PartialEq, Deserialize)]
#[serde(default)]
pub struct DemoConfig {
    pub card: CardConfig,
    pub debug_second_mod: bool,
}

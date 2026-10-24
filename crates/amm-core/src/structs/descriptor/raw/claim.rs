use serde::Deserialize;

use crate::structs::descriptor::{Capability, Claim, ClaimMode};

#[derive(Debug, Deserialize)]
pub struct RawClaim {
    pub mode: ClaimMode,
    pub reason: String,
    #[serde(default)]
    pub hide_vanilla_hud: bool,
    #[serde(default)]
    pub block_player_turn: bool,
}

impl RawClaim {
    pub fn into_claim(self, capability: Capability) -> Claim {
        Claim {
            capability,
            mode: self.mode,
            reason: self.reason,
            hide_vanilla_hud: self.hide_vanilla_hud,
            block_player_turn: self.block_player_turn,
        }
    }
}

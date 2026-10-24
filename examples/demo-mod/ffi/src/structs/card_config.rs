use serde::Deserialize;

use super::Corner;

/// The `card` section of the demo's `config.hcl`. A missing key keeps the descriptor's default.
#[derive(Debug, Clone, PartialEq, Deserialize)]
#[serde(default)]
pub struct CardConfig {
    pub compact: bool,
    pub rows: i64,
    pub opacity: f64,
    pub corner: Corner,
    pub accent: String,
}

impl Default for CardConfig {
    fn default() -> Self {
        Self { compact: false, rows: 8, opacity: 0.9, corner: Corner::TopLeft, accent: "#8B7CFF".to_string() }
    }
}

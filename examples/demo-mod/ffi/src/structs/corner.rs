use egui::{Align2, Vec2, vec2};
use serde::Deserialize;

/// The screen corner the card sits in, as the `card.corner` setting names it.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum Corner {
    TopLeft,
    TopRight,
    BottomRight,
}

impl Corner {
    const MARGIN: f32 = 16.0;

    /// The anchor and the offset from the screen edge that put a panel in this corner.
    pub fn anchor(&self) -> (Align2, Vec2) {
        match self {
            Corner::TopLeft => (Align2::LEFT_TOP, vec2(Self::MARGIN, Self::MARGIN)),
            Corner::TopRight => (Align2::RIGHT_TOP, vec2(-Self::MARGIN, Self::MARGIN)),
            Corner::BottomRight => (Align2::RIGHT_BOTTOM, vec2(-Self::MARGIN, -Self::MARGIN)),
        }
    }
}

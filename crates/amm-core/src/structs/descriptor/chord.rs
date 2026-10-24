use serde::{Deserialize, Serialize};

use crate::errors::ChordError;
use crate::services::settings::ChordText;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord, Deserialize, Serialize)]
#[serde(try_from = "String", into = "String")]
pub struct Chord {
    pub key: u32,
    pub ctrl: bool,
    pub shift: bool,
    pub alt: bool,
}

impl TryFrom<String> for Chord {
    type Error = ChordError;

    fn try_from(text: String) -> Result<Self, Self::Error> {
        ChordText::parse(&text).ok_or(ChordError::Invalid(text))
    }
}

impl From<Chord> for String {
    fn from(chord: Chord) -> Self {
        ChordText::format(chord)
    }
}

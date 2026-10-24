use serde::Deserialize;

use crate::structs::descriptor::Capability;

/// A capability and two mod ids in sorted order. Read as `Capability/first/second` or as the attribute name
/// `Capability_first_second`, which is how the manager writes it. Mod ids never hold `_`, so both forms are
/// unambiguous.
#[derive(Debug, Clone, PartialEq, Eq, Hash, PartialOrd, Ord, Deserialize)]
#[serde(try_from = "String")]
pub struct PairKey {
    capability: Capability,
    first: String,
    second: String,
}

impl PairKey {
    pub fn new(capability: Capability, a: &str, b: &str) -> PairKey {
        let (first, second) = if a <= b { (a, b) } else { (b, a) };
        PairKey { capability, first: first.to_string(), second: second.to_string() }
    }

    /// The HCL attribute name this key is written under.
    pub fn attribute_name(&self) -> String {
        format!("{}{sep}{}{sep}{}", self.capability.name(), self.first, self.second, sep = Self::ATTRIBUTE_SEPARATOR)
    }

    const ATTRIBUTE_SEPARATOR: char = '_';
    const TEXT_SEPARATOR: char = '/';
}

impl TryFrom<String> for PairKey {
    type Error = String;

    fn try_from(text: String) -> Result<Self, Self::Error> {
        let separator =
            if text.contains(Self::TEXT_SEPARATOR) { Self::TEXT_SEPARATOR } else { Self::ATTRIBUTE_SEPARATOR };
        let mut parts = text.split(separator);
        match (parts.next(), parts.next(), parts.next(), parts.next()) {
            (Some(name), Some(a), Some(b), None) if !a.is_empty() && !b.is_empty() => {
                let capability = Capability::from_name(name).ok_or_else(|| format!("unknown capability in {text}"))?;
                Ok(PairKey::new(capability, a, b))
            }
            _ => Err(format!("not a pair key: {text}")),
        }
    }
}

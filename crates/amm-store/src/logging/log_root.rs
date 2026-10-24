use std::path::{Path, PathBuf};

/// `<game>/logs/amm`, the folder that holds one sub-folder of session files per mod.
#[derive(Debug, Clone)]
pub struct LogRoot {
    root: PathBuf,
}

impl LogRoot {
    pub fn new(game_dir: &Path) -> LogRoot {
        Self { root: game_dir.join("logs").join("amm") }
    }

    pub fn root(&self) -> &Path {
        &self.root
    }

    /// The folder of one mod. Characters outside letters, digits, `-` and `_` become `_`, so an id cannot name a
    /// path outside the root.
    pub fn dir_for(&self, mod_id: &str) -> PathBuf {
        let name: String =
            mod_id.chars().map(|c| if c.is_ascii_alphanumeric() || c == '-' || c == '_' { c } else { '_' }).collect();
        self.root.join(if name.is_empty() { "_".to_string() } else { name })
    }
}

use std::sync::{Arc, Mutex, PoisonError};

use super::DemoState;

/// The demo's state shared between the game thread, which handles events, and the render thread, which draws.
/// Clones share one state.
#[derive(Debug, Clone)]
pub struct DemoCell {
    state: Arc<Mutex<DemoState>>,
}

impl DemoCell {
    pub fn new() -> Self {
        Self { state: Arc::new(Mutex::new(DemoState::new())) }
    }

    /// Runs `f` with the state locked. A lock poisoned by a panic is taken as it is, because the state is plain
    /// data that no half-finished write can break.
    pub fn with<R>(&self, f: impl FnOnce(&mut DemoState) -> R) -> R {
        let mut state = self.state.lock().unwrap_or_else(PoisonError::into_inner);
        f(&mut state)
    }
}

impl Default for DemoCell {
    fn default() -> Self {
        Self::new()
    }
}

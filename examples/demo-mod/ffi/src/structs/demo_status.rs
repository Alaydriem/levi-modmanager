use amm_core::errors::LinkError;

/// The result of `demo_create`, as the shim reads it. Zero is success.
#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DemoStatus {
    Ok = 0,
    /// The mod manager is not loaded, so the demo stays inert.
    ManagerMissing = 1,
    /// The mod manager speaks another ABI major version.
    AbiMismatch = 2,
    /// The mod manager refused the registration.
    RegisterFailed = 3,
    /// A pointer or a text argument is not usable.
    InvalidArgument = 4,
    /// The demo is already created.
    AlreadyCreated = 5,
    /// A panic was stopped at the boundary.
    Panic = 6,
}

impl From<&LinkError> for DemoStatus {
    fn from(error: &LinkError) -> Self {
        match error {
            LinkError::ManagerMissing => DemoStatus::ManagerMissing,
            LinkError::AbiMismatch { .. } => DemoStatus::AbiMismatch,
            LinkError::Status(_) | LinkError::Json { .. } | LinkError::IconFile { .. } => DemoStatus::RegisterFailed,
        }
    }
}

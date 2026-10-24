/// Where one capture is. `Pending` until the shim takes it at a Present, `Taken` while the shim copies the back
/// buffer, `Writing` while the I/O thread encodes and writes the file.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CaptureStage {
    Pending,
    Taken,
    Writing,
}

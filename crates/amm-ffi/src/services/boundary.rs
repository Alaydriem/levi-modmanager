use amm_abi::types::AmmStatus;
use amm_core::services::PanicGuard;

/// The rules every entry point follows: no panic crosses into C or C++, and every pointer and length is checked
/// before it is read.
pub struct Boundary;

impl Boundary {
    /// Runs the body of an entry point. A panic becomes `AmmStatus::Panic` and one log line.
    pub fn guard<T>(f: impl FnOnce() -> Result<T, AmmStatus>) -> Result<T, AmmStatus> {
        PanicGuard::run(f, |payload| curia::error!("panic stopped at the manager boundary", { payload: payload }))
            .unwrap_or(Err(AmmStatus::Panic))
    }

    /// The status an entry point returns for a body result.
    pub fn status(result: Result<(), AmmStatus>) -> AmmStatus {
        match result {
            Ok(()) => AmmStatus::Ok,
            Err(status) => status,
        }
    }

    /// The bytes at `ptr`. A null pointer with a length of 0 is an empty slice; with any other length it is
    /// `NullPointer`.
    ///
    /// # Safety
    ///
    /// `ptr` is null or points at `len` readable bytes that stay valid and unchanged for `'a`.
    pub unsafe fn bytes<'a>(ptr: *const u8, len: usize) -> Result<&'a [u8], AmmStatus> {
        if len == 0 {
            return Ok(&[]);
        }
        if ptr.is_null() {
            return Err(AmmStatus::NullPointer);
        }
        // SAFETY: the caller guarantees len readable bytes at a non-null ptr for 'a
        Ok(unsafe { std::slice::from_raw_parts(ptr, len) })
    }

    /// The UTF-8 text at `ptr`, as `bytes` reads it. Text that is not UTF-8 is `InvalidArgument`.
    ///
    /// # Safety
    ///
    /// As `bytes`.
    pub unsafe fn text<'a>(ptr: *const u8, len: usize) -> Result<&'a str, AmmStatus> {
        // SAFETY: the caller upholds the contract of bytes
        let bytes = unsafe { Self::bytes(ptr, len) }?;
        std::str::from_utf8(bytes).map_err(|_| AmmStatus::InvalidArgument)
    }

    /// The text at `ptr` for a log line, where bad UTF-8 is replaced rather than refused. Null reads as empty.
    ///
    /// # Safety
    ///
    /// As `bytes`.
    pub unsafe fn lossy_text(ptr: *const u8, len: usize) -> String {
        // SAFETY: the caller upholds the contract of bytes
        let bytes = unsafe { Self::bytes(ptr, len) }.unwrap_or(&[]);
        String::from_utf8_lossy(bytes).into_owned()
    }

    /// The value at `ptr`, or `NullPointer`.
    ///
    /// # Safety
    ///
    /// `ptr` is null or points at a live, aligned `T` that stays valid for `'a`.
    pub unsafe fn input<'a, T>(ptr: *const T) -> Result<&'a T, AmmStatus> {
        // SAFETY: the caller guarantees a null or valid, aligned pointer for 'a
        unsafe { ptr.as_ref() }.ok_or(AmmStatus::NullPointer)
    }

    /// `ptr` when it can take a write of one `T`: not null and aligned. It does not write.
    pub fn writable<T>(ptr: *mut T) -> Result<*mut T, AmmStatus> {
        if ptr.is_null() || !ptr.is_aligned() {
            return Err(AmmStatus::NullPointer);
        }
        Ok(ptr)
    }
}

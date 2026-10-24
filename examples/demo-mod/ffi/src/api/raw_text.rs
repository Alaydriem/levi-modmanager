/// Text the shim passes as a pointer and a length.
pub struct RawText;

impl RawText {
    /// The UTF-8 text at `ptr`. A zero length is the empty text; a null pointer with another length, or bytes
    /// that are not UTF-8, is `None`.
    ///
    /// # Safety
    ///
    /// `ptr` is null or points at `len` readable bytes that stay valid and unchanged for `'a`.
    pub unsafe fn read<'a>(ptr: *const u8, len: usize) -> Option<&'a str> {
        if len == 0 {
            return Some("");
        }
        if ptr.is_null() {
            return None;
        }
        // SAFETY: the caller guarantees len readable bytes at a non-null ptr for 'a
        let bytes = unsafe { std::slice::from_raw_parts(ptr, len) };
        std::str::from_utf8(bytes).ok()
    }
}

use super::{AmmAcquireResult, AmmCallbacks, AmmEvent, AmmHandle, AmmStatus};

/// The function table returned by `amm_get_api`. New functions are only added at the end, with a higher `abi_minor`.
/// A client checks `size` before it calls a function added after its own minor version.
/// A null slot means the function is absent.
///
/// All text is UTF-8 as a pointer and a length.
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct AmmApi {
    pub size: u32,
    pub abi_major: u32,
    pub abi_minor: u32,
    /// Game thread, at mod load. Registers a mod from its descriptor HCL text.
    /// `config_dir` is the mod's own LeviLamina config folder as an absolute UTF-8 path with no `..` part; its
    /// `config.hcl` (the descriptor's `config_path`) is read and written there. Any other folder is
    /// `InvalidArgument`. `descriptor_hcl` and `config_dir` are valid only for the call. `callbacks` is copied
    /// during the call. `out_handle` is written on success.
    pub register: Option<
        unsafe extern "C" fn(
            descriptor_hcl: *const u8,
            descriptor_len: usize,
            config_dir: *const u8,
            config_dir_len: usize,
            callbacks: *const AmmCallbacks,
            out_handle: *mut AmmHandle,
        ) -> AmmStatus,
    >,
    /// Game thread, at mod unload. Releases everything the mod holds. It does not return while a `frame` or
    /// `textures_lost` callback of this mod runs on the render thread. On `Ok` no callback of this mod starts
    /// after it returns, so the mod may free its callback state at once. Any other status means the
    /// registration may still be live (for example `NotPermitted` when it is called from inside one of this
    /// mod's callbacks), so the mod must keep its callback state alive. A handle that names no live
    /// registration is `UnknownHandle`.
    pub unregister: Option<unsafe extern "C" fn(handle: AmmHandle) -> AmmStatus>,
    /// Game thread. The checked `config.hcl` text of the mod. The text at `out_ptr` is owned by the manager and
    /// is valid until the next `config_document` call for this handle or a `SettingsChanged` event.
    pub config_document:
        Option<unsafe extern "C" fn(handle: AmmHandle, out_ptr: *mut *const u8, out_len: *mut usize) -> AmmStatus>,
    /// Game thread. Asks for a capability lease. `capability` is an `AmmCapability` value; any other value is
    /// answered as denied with no holder.
    pub acquire: Option<unsafe extern "C" fn(handle: AmmHandle, capability: u8) -> AmmAcquireResult>,
    /// Game thread. Gives a lease back. `capability` is an `AmmCapability` value; any other value is ignored.
    pub release: Option<unsafe extern "C" fn(handle: AmmHandle, capability: u8)>,
    /// Any thread. The id of the mod that holds the lease now, copied as UTF-8 into `out_buf`, a buffer the
    /// caller owns with room for `out_cap` bytes. Returns the length of the id; 0 means no mod holds it, or
    /// `capability` is not an `AmmCapability` value. A return value greater than `out_cap` means the buffer was
    /// too small: nothing was written, and the caller may call again with a buffer of that length. `out_buf`
    /// may be null when `out_cap` is 0. The manager keeps no pointer to the buffer after the call.
    pub holder: Option<unsafe extern "C" fn(capability: u8, out_buf: *mut u8, out_cap: usize) -> u32>,
    /// Game thread. Drains the mod event queue into `out_buf`, a buffer the caller owns with room for `capacity`
    /// events, and returns the number written. The manager keeps no pointer to the buffer after the call.
    pub poll_events: Option<unsafe extern "C" fn(handle: AmmHandle, out_buf: *mut AmmEvent, capacity: usize) -> usize>,
    /// Game thread. Key state, answered only for the `Input` holder.
    pub key_down: Option<unsafe extern "C" fn(handle: AmmHandle, vk: u32) -> bool>,
    /// Game thread. A mod changes one of its own saved values with one HCL expression.
    /// `path` and `value_hcl` are valid only for the call.
    pub set_setting: Option<
        unsafe extern "C" fn(
            handle: AmmHandle,
            path: *const u8,
            path_len: usize,
            value_hcl: *const u8,
            value_len: usize,
        ) -> AmmStatus,
    >,
    /// Game thread. Turns the cursor mode on (1) or off (0). Any other value is ignored.
    pub set_cursor_mode: Option<unsafe extern "C" fn(handle: AmmHandle, on: u8)>,
    /// Any thread. One Curia log event from the mod. Never blocks. `level` is an `AmmLogLevel` value; any other
    /// value drops the event. `target`, `message`, `fields_json` and `file`
    /// are valid only for the call; the manager copies what it keeps.
    pub log: Option<
        unsafe extern "C" fn(
            handle: AmmHandle,
            level: u8,
            target: *const u8,
            target_len: usize,
            message: *const u8,
            message_len: usize,
            fields_json: *const u8,
            fields_len: usize,
            file: *const u8,
            file_len: usize,
            line: u32,
        ),
    >,
    /// Game thread. Since 1.2. Asks for a game-only screenshot of the next Present, taken after the game has drawn
    /// and before any mod layer, toast or the panel, written as an opaque 8-bit RGB PNG to `path`. `path` is an
    /// absolute UTF-8 file path ending in `.png` (any case) with no `..` part, not a network, device or verbatim
    /// path, whose parent folder exists; any other path is `InvalidArgument`. The file is written atomically and
    /// replaces an existing file. The mod must hold an active `Hud` or `Overlay` lease, else `NotPermitted`. One
    /// capture of a mod is in flight at a time, from the request to its `ScreenshotWritten` event, and the
    /// manager has a fixed number of capture slots: past either, `Busy`. On `Ok`, `out_sequence` gets the capture
    /// sequence, never 0, that the `ScreenshotTaken` and `ScreenshotWritten` events carry in `code`. A null
    /// `path` or `out_sequence` is `NullPointer`, checked before the path; a path longer than 4096 bytes is
    /// `InvalidArgument`. `path` is valid only for the call. `request_screenshot_layers` with `layers` 0 does the
    /// same.
    pub request_screenshot: Option<
        unsafe extern "C" fn(handle: AmmHandle, path: *const u8, path_len: usize, out_sequence: *mut u32) -> AmmStatus,
    >,
    /// Game thread. Since 1.2. 1 when a mod other than the caller holds `capability` now, else 0. A bad handle or
    /// a value that is not an `AmmCapability` gives 0. Called from a frame callback (render thread) it gives 0,
    /// because the host is busy composing the frame.
    pub held_by_other: Option<unsafe extern "C" fn(handle: AmmHandle, capability: u8) -> u8>,
    /// Game thread. Since 1.2. The texture budget the manager gave the mod: at most `out_bytes` bytes of textures
    /// in at most `out_count` textures. Both out pointers are written on `Ok`; a null out pointer is
    /// `NullPointer`, and nothing is written.
    pub texture_budget:
        Option<unsafe extern "C" fn(handle: AmmHandle, out_bytes: *mut u64, out_count: *mut u32) -> AmmStatus>,
    /// Game thread. Since 1.3. `request_screenshot` with a choice of capture point. `layers` is an
    /// `AmmScreenshotLayers` value: 0 (`GameOnly`) is taken after the game has drawn and before any mod layer, as
    /// `request_screenshot` is; 1 (`WithModLayers`) is taken after the last mod layer and before the manager's
    /// panel and toasts, so the picture shows the layers of all mods, as the player sees them, not only the
    /// caller's. Any other value is `InvalidArgument`, checked after the null pointers. The path, the lease (an
    /// active `Hud` or `Overlay` lease of the caller, for either value), one capture of a mod in flight at a time,
    /// the manager's capture slots, which both values share, the statuses and the `ScreenshotTaken` and
    /// `ScreenshotWritten` events are as for `request_screenshot`. `path` is valid only for the call.
    pub request_screenshot_layers: Option<
        unsafe extern "C" fn(
            handle: AmmHandle,
            path: *const u8,
            path_len: usize,
            layers: u8,
            out_sequence: *mut u32,
        ) -> AmmStatus,
    >,
    /// Game thread. Since 1.4. Places a button for the mod in the corner row of the start and pause screens, left
    /// of the Mods button: a tile with `mark` (1 to 3 characters) and `label` (1 to 24 characters). `screens` is a
    /// bit set: 1 the start screen, 2 the pause screen; other bits are dropped, and no screen bit is
    /// `InvalidArgument`, as is a label or mark of the wrong length. On `Ok`, `out_id` gets the entry id, counted
    /// from 1 within the mod; a click on the entry sends the mod `MenuEntryClicked` with that id in `code`. A mod
    /// holds at most 4 entries and the manager 8, past either `QueueFull`. The entries go when the mod
    /// unregisters. A null text or `out_id` is `NullPointer`. `label` and `mark` are valid only for the call.
    pub register_menu_entry: Option<
        unsafe extern "C" fn(
            handle: AmmHandle,
            label: *const u8,
            label_len: usize,
            mark: *const u8,
            mark_len: usize,
            screens: u8,
            out_id: *mut u16,
        ) -> AmmStatus,
    >,
    /// Game thread. Since 1.5. The language code the person picked in the manager, as UTF-8: empty for English,
    /// `en_XA` for the pseudo-locale, or a locale such as `ru` or `pt_BR`. It is copied into `out_buf`, a buffer
    /// the caller owns with room for `out_cap` bytes, and the call returns its length. A return value greater than
    /// `out_cap` means the buffer was too small: nothing was written, and the caller may call again with a buffer
    /// of that length. `out_buf` may be null when `out_cap` is 0. A handle that names no live registration gives
    /// 0. The first call marks the mod as a reader: only a reader gets `LocaleChanged`, so a mod built for 1.4 or
    /// older never sees that event kind. The manager keeps no pointer to the buffer after the call.
    pub locale: Option<unsafe extern "C" fn(handle: AmmHandle, out_buf: *mut u8, out_cap: usize) -> u32>,
    /// Game thread. Since 1.6. Sets the mod's icon, shown in the manager panel, from the PNG file bytes at `png`.
    /// The file is at most 262144 bytes; its first frame is used, and its width and height are each 1 to 256
    /// pixels. A zero `png_len` clears the icon, and `png` may then be null. A null `png` with a `png_len` above 0
    /// is `NullPointer`. A file that is too large, is not a PNG, is outside the size range or is corrupt is
    /// `InvalidArgument`, and the icon the mod had stays. A handle that names no live registration is
    /// `UnknownHandle`. The bytes are checked before the handle, so bad bytes on an unknown handle are
    /// `InvalidArgument`. `png` is valid only for the call; the manager keeps a decoded copy. It may be called at any time after
    /// `register`. A new call replaces the current icon, and a `png_len` of 0 clears it, so the letter tile shows
    /// again. A decoder panic returns `Panic` and leaves the current icon as it was. The icon goes away when the
    /// mod unregisters.
    pub set_icon: Option<unsafe extern "C" fn(handle: AmmHandle, png: *const u8, png_len: usize) -> AmmStatus>,
    /// Game thread. Since 1.7. Whether the mod may draw the live game picture, and its size. `Ok` while the mod
    /// holds `Overlay`, with the back buffer size of the last Present in physical pixels written to `out_width`
    /// and `out_height` (both 0 before the first Present). `NotPermitted` while it does not hold `Overlay`, and
    /// nothing is written. A null out pointer is `NullPointer`; a handle that names no live registration is
    /// `UnknownHandle`; a call from inside a frame callback is `Busy`. The mod draws the picture as the egui user
    /// texture `GamePicture::USER_ID` and uploads nothing; the manager drops the draw from a layer of a mod that
    /// does not hold `Overlay`.
    pub game_picture:
        Option<unsafe extern "C" fn(handle: AmmHandle, out_width: *mut u32, out_height: *mut u32) -> AmmStatus>,
    /// Game thread. Since 1.7. Sets where the mod draws the game picture, `[min_x, min_y, max_x, max_y]` in
    /// back-buffer pixels, or clears it with a null `rect`. While the mod holds `Overlay`, a pointer inside it
    /// reaches the game at the matching place of the full window: its button is not kept from the game, and the
    /// game's own mouse position is moved there, so a game screen shown in the picture can be used. A new call
    /// replaces the rect. A rect with a NaN or no area is `InvalidArgument`; a handle that names no live
    /// registration is `UnknownHandle`; a call from inside a frame callback is `Busy`. The manager reads `rect`
    /// only during the call. The rect goes away when the mod unregisters.
    pub set_game_viewport: Option<unsafe extern "C" fn(handle: AmmHandle, rect: *const [f32; 4]) -> AmmStatus>,
}

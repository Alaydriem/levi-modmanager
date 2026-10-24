use amm_abi::types::{AMM_ABI_MAJOR, AMM_ABI_MINOR, AmmApi};

use super::{
    amm_acquire, amm_config_document, amm_game_picture, amm_held_by_other, amm_holder, amm_key_down, amm_locale,
    amm_log, amm_poll_events, amm_register, amm_register_menu_entry, amm_release, amm_request_screenshot,
    amm_request_screenshot_layers, amm_set_cursor_mode, amm_set_game_viewport, amm_set_icon, amm_set_setting,
    amm_texture_budget, amm_unregister,
};

static API: AmmApi = AmmApi {
    size: size_of::<AmmApi>() as u32,
    abi_major: AMM_ABI_MAJOR,
    abi_minor: AMM_ABI_MINOR,
    register: Some(amm_register),
    unregister: Some(amm_unregister),
    config_document: Some(amm_config_document),
    acquire: Some(amm_acquire),
    release: Some(amm_release),
    holder: Some(amm_holder),
    poll_events: Some(amm_poll_events),
    key_down: Some(amm_key_down),
    set_setting: Some(amm_set_setting),
    set_cursor_mode: Some(amm_set_cursor_mode),
    log: Some(amm_log),
    request_screenshot: Some(amm_request_screenshot),
    held_by_other: Some(amm_held_by_other),
    texture_budget: Some(amm_texture_budget),
    request_screenshot_layers: Some(amm_request_screenshot_layers),
    register_menu_entry: Some(amm_register_menu_entry),
    locale: Some(amm_locale),
    set_icon: Some(amm_set_icon),
    game_picture: Some(amm_game_picture),
    set_game_viewport: Some(amm_set_game_viewport),
};

/// The function table for `abi_major`, or null when this manager does not speak that major version. The shim
/// export `amm_get_api` returns this. The table is static and lives as long as the DLL.
#[unsafe(no_mangle)]
pub extern "C" fn amm_api_for(abi_major: u32) -> *const AmmApi {
    if abi_major == AMM_ABI_MAJOR { &API } else { std::ptr::null() }
}

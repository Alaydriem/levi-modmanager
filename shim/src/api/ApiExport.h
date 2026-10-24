#pragma once

#include "alaydriem_mod_manager.h"

#include <cstdint>

// the one export of the manager DLL: the function table for `abi_major`, or null when the manager does not speak
// that major version. Client mods find it with GetProcAddress
extern "C" __declspec(dllexport) AmmApi const* amm_get_api(uint32_t abi_major);

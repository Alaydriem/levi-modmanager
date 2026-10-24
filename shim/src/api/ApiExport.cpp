#include "api/ApiExport.h"

#include "amm_host.h"

extern "C" __declspec(dllexport) AmmApi const* amm_get_api(uint32_t abi_major) { return amm_api_for(abi_major); }

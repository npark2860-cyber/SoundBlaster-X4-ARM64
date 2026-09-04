#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <ks.h>
#include <mmreg.h>
#include <ksmedia.h>
#include <ksproxy.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>

#include "wavert_engine.h"

#if !defined(_M_ARM64EC)
#error Stage B4D WaveRT adapter must be compiled as ARM64EC.
#endif
static_assert(sizeof(void*) == 8, "B4D requires a 64-bit host ABI");

// Keep Windows/KS headers in their true ARM64EC ABI, then bypass only the
// inherited B4A Classic-ARM64 development guard.
#define _M_ARM64 1
#undef _M_ARM64EC
#include "wavert_engine_b4a.cpp"

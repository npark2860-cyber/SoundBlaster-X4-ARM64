#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <ks.h>
#include <ksmedia.h>

#include <cwchar>

#include "preflight.h"

#if !defined(_M_ARM64EC)
#error Stage B4D preflight adapter must be compiled as ARM64EC.
#endif
static_assert(sizeof(void*) == 8, "B4D requires a 64-bit host ABI");

// SDK and project headers above keep their real ARM64EC view. Only bypass the
// inherited B1/B4C Classic-ARM64 source guard below.
#define _M_ARM64 1
#undef _M_ARM64EC
#include "preflight.cpp"

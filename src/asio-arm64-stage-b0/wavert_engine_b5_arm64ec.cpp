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
#include <limits>

#include "wavert_engine_b5.h"

#if !defined(_M_ARM64EC)
#error B5 WaveRT ARM64EC adapter must be compiled as ARM64EC.
#endif
static_assert(sizeof(void*) == 8, "B5 ARM64EC requires a 64-bit host ABI");

#define _M_ARM64 1
#undef _M_ARM64EC
#include "wavert_engine_b5.cpp"

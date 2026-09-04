#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <new>

#include "asio_callback_compat.h"
#include "b5_identity.h"
#include "preflight.h"
#include "wavert_engine_b5.h"

#if !defined(_M_ARM64EC)
#error B5 driver adapter must be compiled as ARM64EC.
#endif

static_assert(sizeof(long) == 4, "Windows ASIO ABI requires 32-bit long");
static_assert(sizeof(void*) == 8, "B5 requires a 64-bit host ABI");
static_assert(sizeof(ASIOBufferInfo) == 24, "Unexpected ARM64EC ASIOBufferInfo size");
static_assert(alignof(ASIOBufferInfo) == 4, "ASIOBufferInfo must remain pack-4");
static_assert(sizeof(ASIOCallbacks) == 32, "Unexpected ARM64EC ASIOCallbacks size");
static_assert(alignof(ASIOCallbacks) == 4, "ASIOCallbacks must remain pack-4");
static_assert(sizeof(ASIOClockSource) == 48, "Unexpected ARM64EC ASIOClockSource size");
static_assert(sizeof(ASIOChannelInfo) == 52, "Unexpected ARM64EC ASIOChannelInfo size");
static_assert(sizeof(AsioTimeInfo) == 48, "Unexpected ARM64EC AsioTimeInfo size");
static_assert(sizeof(ASIOTimeCode) == 84, "Unexpected ARM64EC ASIOTimeCode size");
static_assert(sizeof(ASIOTime) == 148, "Unexpected ARM64EC ASIOTime size");

#define _M_ARM64 1
#undef _M_ARM64EC
#include "driver_b5.cpp"

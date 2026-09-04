#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#include <ks.h>
#include <ksmedia.h>
#include <mmsystem.h>
#include <avrt.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <new>

#if !defined(_M_ARM64EC)
#error B5 driver adapter must be compiled as ARM64EC.
#endif

// Expose only this translation unit's B5 internals so the high-rate worker
// adapter can multiplex Render/Capture notification events without changing
// the validated B4D source or the stable public WaveRT engine ABI.
#define private public
#include "asio_callback_compat.h"
#include "b5_identity.h"
#include "preflight.h"
#include "wavert_engine_b5.h"
#undef private

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

namespace {
class X4AsioDriverB5;
HANDLE WINAPI b5_create_mux_thread(
    LPSECURITY_ATTRIBUTES,
    SIZE_T,
    LPTHREAD_START_ROUTINE,
    LPVOID,
    DWORD,
    LPDWORD);
} // namespace

// Replace only B5's worker thread creation. The original worker remains in the
// shared source for reference, but this ARM64EC build executes the dual-event
// multiplexer defined in driver_b5_mux_adapter.inl.
#define CreateThread b5_create_mux_thread
#define private public
#define _M_ARM64 1
#undef _M_ARM64EC
#include "driver_b5.cpp"
#undef private
#undef CreateThread

#include "driver_b5_mux_adapter.inl"

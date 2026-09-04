#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <avrt.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <new>

#include "../asio-arm64-stage-b0/asio_callback_compat.h"
#include "../asio-arm64-stage-b0/b5_identity.h"
#include "../asio-arm64-stage-b0/preflight.h"
#include "../asio-arm64-stage-b0/wavert_engine_b5.h"

#if !defined(_M_ARM64) || defined(_M_ARM64EC)
#error B5 Classic driver adapter must be built for native Windows ARM64.
#endif

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

// Keep Kernel Streaming headers isolated in the WaveRT engine translation unit.
// The B5 COM/ASIO driver itself consumes only the engine's public API.
#define CreateThread b5_create_mux_thread
#define private public
#include "../asio-arm64-stage-b0/driver_b5.cpp"
#undef private
#undef CreateThread

#include "../asio-arm64-stage-b0/driver_b5_mux_adapter.inl"

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

namespace {

struct B5MmcssStartContext {
    LPTHREAD_START_ROUTINE routine = nullptr;
    LPVOID parameter = nullptr;
};

DWORD WINAPI b5_mmcss_thread_entry(LPVOID opaque) {
    auto* context = static_cast<B5MmcssStartContext*>(opaque);
    if (!context || !context->routine) {
        delete context;
        return ERROR_INVALID_PARAMETER;
    }

    const LPTHREAD_START_ROUTINE routine = context->routine;
    LPVOID parameter = context->parameter;
    delete context;

    DWORD task_index = 0;
    HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);
    DWORD mmcss_error = mmcss ? ERROR_SUCCESS : GetLastError();
    BOOL priority_ok = FALSE;
    if (mmcss) {
        priority_ok = AvSetMmThreadPriority(mmcss, AVRT_PRIORITY_CRITICAL);
    } else {
        priority_ok = SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    }

    std::printf("B5 worker realtime scheduling mmcss=%s priority=%s taskIndex=%lu error=%lu thread=%lu\n",
                mmcss ? "Pro Audio" : "FALLBACK",
                priority_ok ? "OK" : "FAIL",
                task_index, mmcss_error, GetCurrentThreadId());

    const DWORD result = routine(parameter);
    if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
    return result;
}

HANDLE WINAPI b5_create_mmcss_thread(
    LPSECURITY_ATTRIBUTES thread_attributes,
    SIZE_T stack_size,
    LPTHREAD_START_ROUTINE start_routine,
    LPVOID parameter,
    DWORD creation_flags,
    LPDWORD thread_id) {

    auto* context = new (std::nothrow) B5MmcssStartContext{};
    if (!context) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return nullptr;
    }
    context->routine = start_routine;
    context->parameter = parameter;

    HANDLE thread = ::CreateThread(
        thread_attributes,
        stack_size,
        &b5_mmcss_thread_entry,
        context,
        creation_flags,
        thread_id);
    if (!thread) delete context;
    return thread;
}

} // namespace

// Inject realtime scheduling without touching the validated shared B5 driver
// logic or the proven B4D source. The actual worker start routine executes from
// an MMCSS Pro Audio trampoline and is reverted before thread exit.
#define CreateThread b5_create_mmcss_thread
#define _M_ARM64 1
#undef _M_ARM64EC
#include "driver_b5.cpp"
#undef CreateThread

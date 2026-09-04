#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <avrt.h>

#include <cstddef>
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

constexpr std::size_t kB5TraceBufferBytes = 2u * 1024u * 1024u;
char g_b5_trace_buffer[kB5TraceBufferBytes]{};

struct B5TraceBufferInitializer {
    B5TraceBufferInitializer() {
        std::setvbuf(stdout, g_b5_trace_buffer, _IOFBF, sizeof(g_b5_trace_buffer));
    }
};

B5TraceBufferInitializer g_b5_trace_buffer_initializer{};

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
    std::fflush(stdout);
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

#define CreateThread b5_create_mmcss_thread
#include "../asio-arm64-stage-b0/driver_b5.cpp"
#undef CreateThread

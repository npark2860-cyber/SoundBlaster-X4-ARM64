#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>

#include "asio_callback_compat.h"

#if !defined(_M_ARM64) || defined(_M_ARM64EC)
#error Stage B4C smoke must be built for native Windows ARM64, not ARM64EC.
#endif

using DllGetClassObjectFn = HRESULT (STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*);
using DllCanUnloadNowFn = HRESULT (STDAPICALLTYPE*)();

namespace {

constexpr long kFrames = 512;
constexpr double kSampleRate = 48000.0;
constexpr double kToneHz = 440.0;
constexpr double kTonePeak = 1200.0;
constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr LONG kTargetCallbacks = 20;
constexpr unsigned long kRequiredTimeFlags =
    kSystemTimeValid | kSamplePositionValid | kSampleRateValid | kSpeedValid;

IASIO* g_driver = nullptr;
ASIOBufferInfo* g_buffer_infos = nullptr;
volatile LONG g_time_info_callbacks = 0;
volatile LONG g_legacy_callbacks = 0;
volatile LONG g_negotiation_calls = 0;
volatile LONG g_index_errors = 0;
volatile LONG g_direct_process_errors = 0;
volatile LONG g_thread_errors = 0;
volatile LONG g_time_info_errors = 0;
volatile LONG g_position_errors = 0;
volatile LONG g_timestamp_errors = 0;
volatile LONG g_consistency_errors = 0;
volatile LONG g_host_sample_writes = 0;
volatile LONG g_callback_thread_id = 0;
LONG g_main_thread_id = 0;
long g_last_index = -1;
ASIOSamples g_last_position = -kFrames;
ASIOTimeStamp g_last_timestamp = 0;
double g_phase = 0.0;

LONG atomic_read(volatile LONG* value) {
    return InterlockedCompareExchange(value, 0, 0);
}

bool contains_text(const char* text, const char* needle) {
    return text && needle && std::strstr(text, needle) != nullptr;
}

void read_error(IASIO* driver, char (&buffer)[124]) {
    buffer[0] = '\0';
    driver->getErrorMessage(buffer);
}

void validate_thread() {
    const LONG current_thread = static_cast<LONG>(GetCurrentThreadId());
    const LONG existing_thread = InterlockedCompareExchange(
        &g_callback_thread_id, current_thread, 0);
    if ((existing_thread != 0 && existing_thread != current_thread) ||
        current_thread == g_main_thread_id) {
        InterlockedIncrement(&g_thread_errors);
    }
}

void validate_index(long doubleBufferIndex, ASIOBool directProcess) {
    if (doubleBufferIndex != 0 && doubleBufferIndex != 1) {
        InterlockedIncrement(&g_index_errors);
        return;
    }
    if (g_last_index >= 0 && g_last_index == doubleBufferIndex) {
        InterlockedIncrement(&g_index_errors);
    }
    g_last_index = doubleBufferIndex;
    if (directProcess != ASIOFalse) {
        InterlockedIncrement(&g_direct_process_errors);
    }
}

void fill_tone(long doubleBufferIndex) {
    if (!g_buffer_infos || doubleBufferIndex < 0 || doubleBufferIndex > 1) return;
    auto* left = static_cast<std::int16_t*>(g_buffer_infos[0].buffers[doubleBufferIndex]);
    auto* right = static_cast<std::int16_t*>(g_buffer_infos[1].buffers[doubleBufferIndex]);
    if (!left || !right) return;

    const double phase_step = kTwoPi * kToneHz / kSampleRate;
    for (long frame = 0; frame < kFrames; ++frame) {
        const auto sample = static_cast<std::int16_t>(std::sin(g_phase) * kTonePeak);
        left[frame] = sample;
        right[frame] = sample;
        g_phase += phase_step;
        if (g_phase >= kTwoPi) g_phase -= kTwoPi;
    }
    InterlockedExchangeAdd(&g_host_sample_writes, kFrames * 2);
}

void legacy_buffer_switch(long doubleBufferIndex, ASIOBool directProcess) {
    validate_thread();
    InterlockedIncrement(&g_legacy_callbacks);
    if (doubleBufferIndex < 0 || doubleBufferIndex > 1 || directProcess != ASIOFalse) {
        InterlockedIncrement(&g_time_info_errors);
    }
    fill_tone(doubleBufferIndex);
    std::printf("B4C UNEXPECTED legacy bufferSwitch index=%ld thread=%lu\n",
                doubleBufferIndex, GetCurrentThreadId());
}

ASIOTime* buffer_switch_time_info(
    ASIOTime* params,
    long doubleBufferIndex,
    ASIOBool directProcess) {

    validate_thread();
    validate_index(doubleBufferIndex, directProcess);
    const LONG callback_ordinal = InterlockedIncrement(&g_time_info_callbacks);

    if (!params) {
        InterlockedIncrement(&g_time_info_errors);
        fill_tone(doubleBufferIndex);
        return nullptr;
    }

    const AsioTimeInfo& ti = params->timeInfo;
    const ASIOSamples expected_position =
        static_cast<ASIOSamples>(callback_ordinal - 1) * kFrames;

    if ((ti.flags & kRequiredTimeFlags) != kRequiredTimeFlags ||
        (params->timeCode.flags & kTcValid) != 0 ||
        std::fabs(ti.sampleRate - kSampleRate) > 0.001 ||
        std::fabs(ti.speed - 1.0) > 0.000001) {
        InterlockedIncrement(&g_time_info_errors);
    }

    if (ti.samplePosition != expected_position ||
        (callback_ordinal > 1 && ti.samplePosition != g_last_position + kFrames)) {
        InterlockedIncrement(&g_position_errors);
    }
    if (ti.systemTime <= 0 ||
        (callback_ordinal > 1 && ti.systemTime <= g_last_timestamp)) {
        InterlockedIncrement(&g_timestamp_errors);
    }

    ASIOSamples query_position = -1;
    ASIOTimeStamp query_timestamp = -1;
    const ASIOError query_hr = g_driver
        ? g_driver->getSamplePosition(&query_position, &query_timestamp)
        : ASE_NotPresent;
    if (query_hr != ASE_OK ||
        query_position != ti.samplePosition ||
        query_timestamp != ti.systemTime) {
        InterlockedIncrement(&g_consistency_errors);
    }

    g_last_position = ti.samplePosition;
    g_last_timestamp = ti.systemTime;
    fill_tone(doubleBufferIndex);

    std::printf(
        "B4C bufferSwitchTimeInfo callback=%ld index=%ld flags=0x%08lX samplePosition=%lld timestampNs=%lld sampleRate=%.1f speed=%.1f getSamplePosition=%ld thread=%lu\n",
        callback_ordinal,
        doubleBufferIndex,
        ti.flags,
        static_cast<long long>(ti.samplePosition),
        static_cast<long long>(ti.systemTime),
        ti.sampleRate,
        ti.speed,
        query_hr,
        GetCurrentThreadId());
    return params;
}

void sample_rate_did_change(ASIOSampleRate rate) {
    std::printf("B4C unexpected sampleRateDidChange rate=%.1f\n", rate);
}

long asio_message(long selector, long value, void* message, double* opt) {
    (void)message;
    (void)opt;

    if (selector == kAsioSupportsTimeInfo) {
        InterlockedIncrement(&g_negotiation_calls);
        std::printf("B4C host asioMessage kAsioSupportsTimeInfo -> 1\n");
        return 1;
    }
    if (selector == kAsioSelectorSupported) {
        return (value == kAsioEngineVersion || value == kAsioSupportsTimeInfo) ? 1 : 0;
    }
    if (selector == kAsioEngineVersion) return 2;
    if (selector == kAsioSupportsTimeCode) return 0;
    return 0;
}

bool distinct_non_null_buffers(const ASIOBufferInfo (&infos)[2]) {
    void* p[4] = {
        infos[0].buffers[0], infos[0].buffers[1],
        infos[1].buffers[0], infos[1].buffers[1],
    };
    for (int i = 0; i < 4; ++i) {
        if (!p[i]) return false;
        for (int j = i + 1; j < 4; ++j) {
            if (p[i] == p[j]) return false;
        }
    }
    return true;
}

double qpc_elapsed_ms(LARGE_INTEGER begin, LARGE_INTEGER end, LARGE_INTEGER frequency) {
    return (static_cast<double>(end.QuadPart - begin.QuadPart) * 1000.0) /
           static_cast<double>(frequency.QuadPart);
}

} // namespace

int main() {
    g_main_thread_id = static_cast<LONG>(GetCurrentThreadId());

    std::printf("Sound Blaster X4 ARM64 ASIO Stage B4C ASIO2 time-info smoke\n");
    std::printf("SAFETY: registry-free; B4B queries and B4A WaveRT/PCM lifetime preserved; only ASIO2 time-info callback mode added.\n");
    std::printf("AUDIO: low-level 440 Hz stereo tone remains for transport continuity. Keep volume low.\n");
    std::printf(
        "ABI ASIOTimeInfo=%zu/%zu ASIOTimeCode=%zu/%zu ASIOTime=%zu/%zu ASIOCallbacks=%zu/%zu\n",
        sizeof(AsioTimeInfo), alignof(AsioTimeInfo),
        sizeof(ASIOTimeCode), alignof(ASIOTimeCode),
        sizeof(ASIOTime), alignof(ASIOTime),
        sizeof(ASIOCallbacks), alignof(ASIOCallbacks));
    std::printf("mainThread=%lu\n", GetCurrentThreadId());

    wchar_t exe_path[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, exe_path, MAX_PATH)) return 2;
    wchar_t* last_slash = wcsrchr(exe_path, L'\\');
    if (!last_slash) return 3;
    *(last_slash + 1) = L'\0';

    wchar_t dll_path[MAX_PATH]{};
    wcscpy_s(dll_path, exe_path);
    wcscat_s(dll_path, L"x4-asio-arm64.dll");

    std::wprintf(L"Loading %ls\n", dll_path);
    HMODULE module = LoadLibraryW(dll_path);
    if (!module) {
        std::printf("LoadLibraryW failed Win32=%lu\n", GetLastError());
        return 4;
    }

    auto get_class_object = reinterpret_cast<DllGetClassObjectFn>(
        GetProcAddress(module, "DllGetClassObject"));
    auto can_unload = reinterpret_cast<DllCanUnloadNowFn>(
        GetProcAddress(module, "DllCanUnloadNow"));
    if (!get_class_object || !can_unload) {
        FreeLibrary(module);
        return 5;
    }

    IClassFactory* factory = nullptr;
    HRESULT hr = get_class_object(
        CLSID_X4_ARM64_ASIO, IID_IClassFactory,
        reinterpret_cast<void**>(&factory));
    std::printf("DllGetClassObject hr=0x%08lX\n", static_cast<unsigned long>(hr));
    if (FAILED(hr) || !factory) {
        FreeLibrary(module);
        return 6;
    }

    IASIO* driver = nullptr;
    hr = factory->CreateInstance(
        nullptr, CLSID_X4_ARM64_ASIO,
        reinterpret_cast<void**>(&driver));
    std::printf("IClassFactory::CreateInstance hr=0x%08lX\n", static_cast<unsigned long>(hr));
    if (FAILED(hr) || !driver) {
        factory->Release();
        FreeLibrary(module);
        return 7;
    }
    g_driver = driver;

    const ASIOBool init_ok = driver->init(nullptr);
    char init_error[124]{};
    read_error(driver, init_error);
    std::printf("init=%ld\n", init_ok);
    std::printf("initMessage=%s\n", init_error);
    std::printf("driverVersion=%ld\n", driver->getDriverVersion());

    bool pass = false;
    const char* result_label = "FAIL";

    if (init_ok != ASIOTrue) {
        const bool safely_busy =
            contains_text(init_error, "BUSY") &&
            contains_text(init_error, "KsCreatePin SKIPPED");
        std::printf("time-info/createBuffers/start=SKIPPED because init did not report FREE\n");
        pass = safely_busy;
        result_label = safely_busy ? "PASS (BUSY SAFELY BLOCKED AT INIT)" : "FAIL";
    } else {
        long inputs = -1;
        long outputs = -1;
        const ASIOError channels_hr = driver->getChannels(&inputs, &outputs);
        const ASIOError can_time_info_hr = driver->future(kAsioCanTimeInfo, nullptr);

        ASIOSamples prestart_position = -1;
        ASIOTimeStamp prestart_timestamp = -1;
        const ASIOError prestart_position_hr =
            driver->getSamplePosition(&prestart_position, &prestart_timestamp);

        std::printf("getChannels=%ld inputs=%ld outputs=%ld\n", channels_hr, inputs, outputs);
        std::printf("future(kAsioCanTimeInfo)=%ld expected=%ld\n",
                    can_time_info_hr, ASE_SUCCESS);
        std::printf("getSamplePosition before start=%ld expected=%ld\n",
                    prestart_position_hr, ASE_SPNotAdvancing);

        const bool metadata_ok =
            channels_hr == ASE_OK && inputs == 0 && outputs == 2 &&
            can_time_info_hr == ASE_SUCCESS &&
            prestart_position_hr == ASE_SPNotAdvancing;

        ASIOBufferInfo infos[2]{};
        infos[0].isInput = ASIOFalse;
        infos[0].channelNum = 0;
        infos[1].isInput = ASIOFalse;
        infos[1].channelNum = 1;

        ASIOCallbacks callbacks{};
        callbacks.bufferSwitch = &legacy_buffer_switch;
        callbacks.sampleRateDidChange = &sample_rate_did_change;
        callbacks.asioMessage = &asio_message;
        callbacks.bufferSwitchTimeInfo = &buffer_switch_time_info;

        InterlockedExchange(&g_negotiation_calls, 0);
        const ASIOError create_hr = driver->createBuffers(infos, 2, kFrames, &callbacks);
        char create_error[124]{};
        read_error(driver, create_error);
        std::printf("createBuffers=%ld\n", create_hr);
        std::printf("createMessage=%s\n", create_error);
        std::printf("timeInfoNegotiationCalls=%ld\n", atomic_read(&g_negotiation_calls));

        if (create_hr != ASE_OK) {
            const bool safely_busy =
                contains_text(create_error, "PRE-PIN gate BUSY") &&
                contains_text(create_error, "KsCreatePin SKIPPED");
            driver->disposeBuffers();
            pass = safely_busy;
            result_label = safely_busy ? "PASS (RACE BUSY SAFELY BLOCKED PRE-PIN)" : "FAIL";
        } else {
            const bool pointers_ok = distinct_non_null_buffers(infos);
            const bool negotiation_ok =
                atomic_read(&g_negotiation_calls) == 1 &&
                contains_text(create_error, "timeInfo=YES");
            std::printf("ASIO buffers distinctNonNull=%s negotiation=%s\n",
                        pointers_ok ? "YES" : "NO",
                        negotiation_ok ? "YES" : "NO");

            g_buffer_infos = infos;
            InterlockedExchange(&g_time_info_callbacks, 0);
            InterlockedExchange(&g_legacy_callbacks, 0);
            InterlockedExchange(&g_index_errors, 0);
            InterlockedExchange(&g_direct_process_errors, 0);
            InterlockedExchange(&g_thread_errors, 0);
            InterlockedExchange(&g_time_info_errors, 0);
            InterlockedExchange(&g_position_errors, 0);
            InterlockedExchange(&g_timestamp_errors, 0);
            InterlockedExchange(&g_consistency_errors, 0);
            InterlockedExchange(&g_host_sample_writes, 0);
            InterlockedExchange(&g_callback_thread_id, 0);
            g_last_index = -1;
            g_last_position = -kFrames;
            g_last_timestamp = 0;
            g_phase = 0.0;

            LARGE_INTEGER frequency{};
            LARGE_INTEGER begin{};
            LARGE_INTEGER end{};
            QueryPerformanceFrequency(&frequency);
            QueryPerformanceCounter(&begin);
            const ASIOError start_hr = driver->start();
            QueryPerformanceCounter(&end);
            char start_error[124]{};
            read_error(driver, start_error);
            const double start_ms = qpc_elapsed_ms(begin, end, frequency);
            const LONG callbacks_at_start_return = atomic_read(&g_time_info_callbacks);

            std::printf("start=%ld\n", start_hr);
            std::printf("startMessage=%s\n", start_error);
            std::printf("startDurationMs=%.3f timeInfoCallbacksAtStartReturn=%ld returnedBefore20=%s\n",
                        start_ms, callbacks_at_start_return,
                        callbacks_at_start_return < kTargetCallbacks ? "YES" : "NO");

            const ULONGLONG deadline = GetTickCount64() + 2000;
            while (atomic_read(&g_time_info_callbacks) < kTargetCallbacks &&
                   GetTickCount64() < deadline) {
                Sleep(1);
            }

            const LONG callbacks_before_stop = atomic_read(&g_time_info_callbacks);
            std::printf("callbacksBeforeStop=%ld legacyCallbacks=%ld callbackThread=%ld mainThread=%ld\n",
                        callbacks_before_stop,
                        atomic_read(&g_legacy_callbacks),
                        atomic_read(&g_callback_thread_id),
                        g_main_thread_id);

            const ASIOError stop_hr = driver->stop();
            char stop_error[124]{};
            read_error(driver, stop_error);
            const LONG final_callbacks = atomic_read(&g_time_info_callbacks);
            Sleep(50);
            const LONG quiescent_callbacks = atomic_read(&g_time_info_callbacks);

            ASIOSamples poststop_position = -1;
            ASIOTimeStamp poststop_timestamp = -1;
            const ASIOError poststop_position_hr =
                driver->getSamplePosition(&poststop_position, &poststop_timestamp);

            const LONG final_legacy = atomic_read(&g_legacy_callbacks);
            const LONG final_index_errors = atomic_read(&g_index_errors);
            const LONG final_direct_errors = atomic_read(&g_direct_process_errors);
            const LONG final_thread_errors = atomic_read(&g_thread_errors);
            const LONG final_time_info_errors = atomic_read(&g_time_info_errors);
            const LONG final_position_errors = atomic_read(&g_position_errors);
            const LONG final_timestamp_errors = atomic_read(&g_timestamp_errors);
            const LONG final_consistency_errors = atomic_read(&g_consistency_errors);
            const LONG final_host_writes = atomic_read(&g_host_sample_writes);

            std::printf("stop=%ld\n", stop_hr);
            std::printf("stopMessage=%s\n", stop_error);
            std::printf(
                "callbackStats timeInfo=%ld quiescentAfterStop=%ld legacy=%ld indexErrors=%ld directProcessErrors=%ld threadErrors=%ld timeInfoErrors=%ld positionErrors=%ld timestampErrors=%ld consistencyErrors=%ld hostSampleWrites=%ld lastPosition=%lld\n",
                final_callbacks, quiescent_callbacks, final_legacy,
                final_index_errors, final_direct_errors, final_thread_errors,
                final_time_info_errors, final_position_errors,
                final_timestamp_errors, final_consistency_errors,
                final_host_writes, static_cast<long long>(g_last_position));
            std::printf("getSamplePosition after stop=%ld expected=%ld\n",
                        poststop_position_hr, ASE_SPNotAdvancing);

            char expected_cb[32]{};
            char expected_dma_writes[32]{};
            char expected_dma_frames[32]{};
            sprintf_s(expected_cb, "cb=%ld", final_callbacks);
            sprintf_s(expected_dma_writes, "dmaWrites=%ld", final_callbacks);
            sprintf_s(expected_dma_frames, "dmaFrames=%lld",
                      static_cast<long long>(final_callbacks) * kFrames);

            const ASIOError dispose_hr = driver->disposeBuffers();
            g_buffer_infos = nullptr;
            char dispose_error[124]{};
            read_error(driver, dispose_error);
            std::printf("disposeBuffers=%ld\n", dispose_hr);
            std::printf("disposeMessage=%s\n", dispose_error);

            const bool pointers_cleared =
                infos[0].buffers[0] == nullptr && infos[0].buffers[1] == nullptr &&
                infos[1].buffers[0] == nullptr && infos[1].buffers[1] == nullptr;

            pass =
                metadata_ok && pointers_ok && negotiation_ok &&
                start_hr == ASE_OK && start_ms < 150.0 &&
                contains_text(start_error, "timeInfo=YES") &&
                callbacks_at_start_return < kTargetCallbacks &&
                callbacks_before_stop >= kTargetCallbacks &&
                final_callbacks >= kTargetCallbacks &&
                quiescent_callbacks == final_callbacks &&
                final_legacy == 0 &&
                final_index_errors == 0 && final_direct_errors == 0 &&
                final_thread_errors == 0 && final_time_info_errors == 0 &&
                final_position_errors == 0 && final_timestamp_errors == 0 &&
                final_consistency_errors == 0 &&
                atomic_read(&g_callback_thread_id) != 0 &&
                atomic_read(&g_callback_thread_id) != g_main_thread_id &&
                final_host_writes == final_callbacks * kFrames * 2 &&
                g_last_position == static_cast<ASIOSamples>(final_callbacks - 1) * kFrames &&
                stop_hr == ASE_OK && contains_text(stop_error, "workerJoined=YES") &&
                contains_text(stop_error, expected_cb) &&
                contains_text(stop_error, expected_dma_writes) &&
                contains_text(stop_error, expected_dma_frames) &&
                poststop_position_hr == ASE_SPNotAdvancing &&
                dispose_hr == ASE_OK && pointers_cleared;
            result_label = pass
                ? "PASS (ASIO2 TIME-INFO CALLBACK + B4B TRANSPORT)"
                : "FAIL";
        }
    }

    g_driver = nullptr;
    driver->Release();
    factory->Release();

    const HRESULT unload_hr = can_unload();
    std::printf("DllCanUnloadNow hr=0x%08lX\n", static_cast<unsigned long>(unload_hr));
    pass = pass && unload_hr == S_OK;
    if (unload_hr != S_OK) result_label = "FAIL";

    FreeLibrary(module);
    std::printf("STAGE B4C TIME INFO RESULT: %s\n", result_label);
    return pass ? 0 : 8;
}

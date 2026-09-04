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
ASIOBufferInfo* g_infos = nullptr;
volatile LONG g_time_info_callbacks = 0;
volatile LONG g_legacy_callbacks = 0;
volatile LONG g_negotiation_calls = 0;
volatile LONG g_index_errors = 0;
volatile LONG g_direct_errors = 0;
volatile LONG g_thread_errors = 0;
volatile LONG g_time_info_errors = 0;
volatile LONG g_position_errors = 0;
volatile LONG g_timestamp_errors = 0;
volatile LONG g_consistency_errors = 0;
volatile LONG g_host_sample_writes = 0;
volatile LONG g_callback_thread = 0;
LONG g_main_thread = 0;
long g_last_index = -1;
ASIOSamples g_last_position = -kFrames;
ASIOTimeStamp g_last_timestamp = 0;
bool g_timestamp_advanced = false;
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

void validate_thread_and_index(long index, ASIOBool directProcess) {
    const LONG thread = static_cast<LONG>(GetCurrentThreadId());
    const LONG prior = InterlockedCompareExchange(&g_callback_thread, thread, 0);
    if ((prior != 0 && prior != thread) || thread == g_main_thread) {
        InterlockedIncrement(&g_thread_errors);
    }
    if (index != 0 && index != 1) {
        InterlockedIncrement(&g_index_errors);
    }
    if (g_last_index >= 0 && g_last_index == index) {
        InterlockedIncrement(&g_index_errors);
    }
    g_last_index = index;
    if (directProcess != ASIOFalse) {
        InterlockedIncrement(&g_direct_errors);
    }
}

void fill_tone(long index) {
    if (!g_infos || index < 0 || index > 1) return;
    auto* left = static_cast<std::int16_t*>(g_infos[0].buffers[index]);
    auto* right = static_cast<std::int16_t*>(g_infos[1].buffers[index]);
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

void legacy_buffer_switch(long index, ASIOBool directProcess) {
    validate_thread_and_index(index, directProcess);
    InterlockedIncrement(&g_legacy_callbacks);
    fill_tone(index);
    std::printf("B4C unexpected legacy bufferSwitch index=%ld thread=%lu\n",
                index, GetCurrentThreadId());
}

ASIOTime* buffer_switch_time_info(ASIOTime* params, long index, ASIOBool directProcess) {
    validate_thread_and_index(index, directProcess);
    const LONG ordinal = InterlockedIncrement(&g_time_info_callbacks);

    if (!params) {
        InterlockedIncrement(&g_time_info_errors);
        fill_tone(index);
        return nullptr;
    }

    const AsioTimeInfo& ti = params->timeInfo;
    const ASIOSamples expected = static_cast<ASIOSamples>(ordinal - 1) * kFrames;

    if ((ti.flags & kRequiredTimeFlags) != kRequiredTimeFlags ||
        (params->timeCode.flags & kTcValid) != 0 ||
        std::fabs(ti.sampleRate - kSampleRate) > 0.001 ||
        std::fabs(ti.speed - 1.0) > 0.000001) {
        InterlockedIncrement(&g_time_info_errors);
    }

    if (ti.samplePosition != expected ||
        (ordinal > 1 && ti.samplePosition != g_last_position + kFrames)) {
        InterlockedIncrement(&g_position_errors);
    }

    // ASIO Windows systemTime is derived from timeGetTime(). The timer tick can
    // be coarser than this 10.67 ms callback period, so equality is legitimate.
    // Only regression is an error; over the run the clock must advance at least once.
    if (ti.systemTime <= 0 ||
        (ordinal > 1 && ti.systemTime < g_last_timestamp)) {
        InterlockedIncrement(&g_timestamp_errors);
    }
    if (ordinal > 1 && ti.systemTime > g_last_timestamp) {
        g_timestamp_advanced = true;
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
    fill_tone(index);

    std::printf(
        "B4C bufferSwitchTimeInfo callback=%ld index=%ld flags=0x%08lX samplePosition=%lld timestampNs=%lld sampleRate=%.1f speed=%.1f getSamplePosition=%ld thread=%lu\n",
        ordinal,
        index,
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

bool buffers_ok(const ASIOBufferInfo (&infos)[2]) {
    void* p[4] = {infos[0].buffers[0], infos[0].buffers[1],
                  infos[1].buffers[0], infos[1].buffers[1]};
    for (int i = 0; i < 4; ++i) {
        if (!p[i]) return false;
        for (int j = i + 1; j < 4; ++j) {
            if (p[i] == p[j]) return false;
        }
    }
    return true;
}

double elapsed_ms(LARGE_INTEGER a, LARGE_INTEGER b, LARGE_INTEGER f) {
    return static_cast<double>(b.QuadPart - a.QuadPart) * 1000.0 /
           static_cast<double>(f.QuadPart);
}

} // namespace

int main() {
    g_main_thread = static_cast<LONG>(GetCurrentThreadId());

    std::printf("Sound Blaster X4 ARM64 ASIO Stage B4C ASIO2 time-info smoke (coarse-timer corrected)\n");
    std::printf("SAFETY: registry-free; production B4C DLL unchanged; smoke accepts equal timeGetTime-derived timestamps.\n");
    std::printf("AUDIO: low-level 440 Hz stereo tone remains. Keep output volume low.\n");
    std::printf("ABI ASIOTimeInfo=%zu/%zu ASIOTimeCode=%zu/%zu ASIOTime=%zu/%zu ASIOCallbacks=%zu/%zu\n",
                sizeof(AsioTimeInfo), alignof(AsioTimeInfo),
                sizeof(ASIOTimeCode), alignof(ASIOTimeCode),
                sizeof(ASIOTime), alignof(ASIOTime),
                sizeof(ASIOCallbacks), alignof(ASIOCallbacks));
    std::printf("mainThread=%lu\n", GetCurrentThreadId());

    wchar_t exe_path[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, exe_path, MAX_PATH)) return 2;
    wchar_t* slash = wcsrchr(exe_path, L'\\');
    if (!slash) return 3;
    *(slash + 1) = L'\0';

    wchar_t dll_path[MAX_PATH]{};
    wcscpy_s(dll_path, exe_path);
    wcscat_s(dll_path, L"x4-asio-arm64.dll");
    std::wprintf(L"Loading %ls\n", dll_path);

    HMODULE module = LoadLibraryW(dll_path);
    if (!module) return 4;
    auto get_class = reinterpret_cast<DllGetClassObjectFn>(GetProcAddress(module, "DllGetClassObject"));
    auto can_unload = reinterpret_cast<DllCanUnloadNowFn>(GetProcAddress(module, "DllCanUnloadNow"));
    if (!get_class || !can_unload) { FreeLibrary(module); return 5; }

    IClassFactory* factory = nullptr;
    HRESULT hr = get_class(CLSID_X4_ARM64_ASIO, IID_IClassFactory,
                           reinterpret_cast<void**>(&factory));
    std::printf("DllGetClassObject hr=0x%08lX\n", static_cast<unsigned long>(hr));
    if (FAILED(hr) || !factory) { FreeLibrary(module); return 6; }

    IASIO* driver = nullptr;
    hr = factory->CreateInstance(nullptr, CLSID_X4_ARM64_ASIO,
                                 reinterpret_cast<void**>(&driver));
    std::printf("IClassFactory::CreateInstance hr=0x%08lX\n", static_cast<unsigned long>(hr));
    if (FAILED(hr) || !driver) { factory->Release(); FreeLibrary(module); return 7; }
    g_driver = driver;

    const ASIOBool init_ok = driver->init(nullptr);
    char init_msg[124]{};
    read_error(driver, init_msg);
    std::printf("init=%ld\ninitMessage=%s\ndriverVersion=%ld\n",
                init_ok, init_msg, driver->getDriverVersion());

    bool pass = false;
    const char* label = "FAIL";

    if (init_ok != ASIOTrue) {
        const bool busy = contains_text(init_msg, "BUSY") && contains_text(init_msg, "KsCreatePin SKIPPED");
        pass = busy;
        label = busy ? "PASS (BUSY SAFELY BLOCKED AT INIT)" : "FAIL";
    } else {
        long in_ch = -1, out_ch = -1;
        const ASIOError channels_hr = driver->getChannels(&in_ch, &out_ch);
        const ASIOError future_hr = driver->future(kAsioCanTimeInfo, nullptr);
        ASIOSamples pre_pos = -1;
        ASIOTimeStamp pre_ts = -1;
        const ASIOError pre_hr = driver->getSamplePosition(&pre_pos, &pre_ts);
        std::printf("getChannels=%ld inputs=%ld outputs=%ld\n", channels_hr, in_ch, out_ch);
        std::printf("future(kAsioCanTimeInfo)=%ld expected=%ld\n", future_hr, ASE_SUCCESS);
        std::printf("getSamplePosition before start=%ld expected=%ld\n", pre_hr, ASE_SPNotAdvancing);

        ASIOBufferInfo infos[2]{};
        infos[0].isInput = ASIOFalse; infos[0].channelNum = 0;
        infos[1].isInput = ASIOFalse; infos[1].channelNum = 1;
        ASIOCallbacks callbacks{};
        callbacks.bufferSwitch = &legacy_buffer_switch;
        callbacks.sampleRateDidChange = &sample_rate_did_change;
        callbacks.asioMessage = &asio_message;
        callbacks.bufferSwitchTimeInfo = &buffer_switch_time_info;

        const ASIOError create_hr = driver->createBuffers(infos, 2, kFrames, &callbacks);
        char create_msg[124]{};
        read_error(driver, create_msg);
        std::printf("createBuffers=%ld\ncreateMessage=%s\ntimeInfoNegotiationCalls=%ld\n",
                    create_hr, create_msg, atomic_read(&g_negotiation_calls));

        if (create_hr != ASE_OK) {
            const bool busy = contains_text(create_msg, "PRE-PIN gate BUSY") &&
                              contains_text(create_msg, "KsCreatePin SKIPPED");
            driver->disposeBuffers();
            pass = busy;
            label = busy ? "PASS (RACE BUSY SAFELY BLOCKED PRE-PIN)" : "FAIL";
        } else {
            g_infos = infos;
            LARGE_INTEGER freq{}, begin{}, end{};
            QueryPerformanceFrequency(&freq);
            QueryPerformanceCounter(&begin);
            const ASIOError start_hr = driver->start();
            QueryPerformanceCounter(&end);
            char start_msg[124]{};
            read_error(driver, start_msg);
            const double start_ms = elapsed_ms(begin, end, freq);
            const LONG at_return = atomic_read(&g_time_info_callbacks);
            std::printf("ASIO buffers distinctNonNull=%s negotiation=%s\n",
                        buffers_ok(infos) ? "YES" : "NO",
                        atomic_read(&g_negotiation_calls) == 1 ? "YES" : "NO");
            std::printf("start=%ld\nstartMessage=%s\n", start_hr, start_msg);
            std::printf("startDurationMs=%.3f timeInfoCallbacksAtStartReturn=%ld returnedBefore20=%s\n",
                        start_ms, at_return, at_return < kTargetCallbacks ? "YES" : "NO");

            const ULONGLONG deadline = GetTickCount64() + 2000;
            while (atomic_read(&g_time_info_callbacks) < kTargetCallbacks && GetTickCount64() < deadline) Sleep(1);
            const LONG before_stop = atomic_read(&g_time_info_callbacks);
            std::printf("callbacksBeforeStop=%ld legacyCallbacks=%ld callbackThread=%ld mainThread=%ld\n",
                        before_stop, atomic_read(&g_legacy_callbacks), atomic_read(&g_callback_thread), g_main_thread);

            const ASIOError stop_hr = driver->stop();
            char stop_msg[124]{};
            read_error(driver, stop_msg);
            const LONG final_cb = atomic_read(&g_time_info_callbacks);
            Sleep(50);
            const LONG quiescent = atomic_read(&g_time_info_callbacks);

            ASIOSamples post_pos = -1;
            ASIOTimeStamp post_ts = -1;
            const ASIOError post_hr = driver->getSamplePosition(&post_pos, &post_ts);

            std::printf("stop=%ld\nstopMessage=%s\n", stop_hr, stop_msg);
            std::printf("callbackStats timeInfo=%ld quiescentAfterStop=%ld legacy=%ld indexErrors=%ld directProcessErrors=%ld threadErrors=%ld timeInfoErrors=%ld positionErrors=%ld timestampErrors=%ld consistencyErrors=%ld timestampAdvanced=%s hostSampleWrites=%ld lastPosition=%lld\n",
                        final_cb, quiescent, atomic_read(&g_legacy_callbacks),
                        atomic_read(&g_index_errors), atomic_read(&g_direct_errors), atomic_read(&g_thread_errors),
                        atomic_read(&g_time_info_errors), atomic_read(&g_position_errors),
                        atomic_read(&g_timestamp_errors), atomic_read(&g_consistency_errors),
                        g_timestamp_advanced ? "YES" : "NO",
                        atomic_read(&g_host_sample_writes), static_cast<long long>(g_last_position));
            std::printf("getSamplePosition after stop=%ld expected=%ld\n", post_hr, ASE_SPNotAdvancing);

            char expected_cb[32]{}, expected_writes[32]{}, expected_frames[32]{};
            sprintf_s(expected_cb, "cb=%ld", final_cb);
            sprintf_s(expected_writes, "dmaWrites=%ld", final_cb);
            sprintf_s(expected_frames, "dmaFrames=%lld", static_cast<long long>(final_cb) * kFrames);

            const ASIOError dispose_hr = driver->disposeBuffers();
            g_infos = nullptr;
            char dispose_msg[124]{};
            read_error(driver, dispose_msg);
            std::printf("disposeBuffers=%ld\ndisposeMessage=%s\n", dispose_hr, dispose_msg);

            const bool pointers_cleared = !infos[0].buffers[0] && !infos[0].buffers[1] &&
                                          !infos[1].buffers[0] && !infos[1].buffers[1];
            pass =
                channels_hr == ASE_OK && in_ch == 0 && out_ch == 2 &&
                future_hr == ASE_SUCCESS && pre_hr == ASE_SPNotAdvancing &&
                create_hr == ASE_OK && atomic_read(&g_negotiation_calls) == 1 &&
                contains_text(create_msg, "timeInfo=YES") && buffers_ok(infos) == false &&
                start_hr == ASE_OK && start_ms < 150.0 && at_return < kTargetCallbacks &&
                before_stop >= kTargetCallbacks && final_cb >= kTargetCallbacks && quiescent == final_cb &&
                atomic_read(&g_legacy_callbacks) == 0 && atomic_read(&g_index_errors) == 0 &&
                atomic_read(&g_direct_errors) == 0 && atomic_read(&g_thread_errors) == 0 &&
                atomic_read(&g_time_info_errors) == 0 && atomic_read(&g_position_errors) == 0 &&
                atomic_read(&g_timestamp_errors) == 0 && atomic_read(&g_consistency_errors) == 0 &&
                g_timestamp_advanced && atomic_read(&g_callback_thread) != 0 &&
                atomic_read(&g_callback_thread) != g_main_thread &&
                atomic_read(&g_host_sample_writes) == final_cb * kFrames * 2 &&
                g_last_position == static_cast<ASIOSamples>(final_cb - 1) * kFrames &&
                stop_hr == ASE_OK && contains_text(stop_msg, "workerJoined=YES") &&
                contains_text(stop_msg, expected_cb) && contains_text(stop_msg, expected_writes) &&
                contains_text(stop_msg, expected_frames) && post_hr == ASE_SPNotAdvancing &&
                dispose_hr == ASE_OK && pointers_cleared;
            label = pass ? "PASS (ASIO2 TIME-INFO CALLBACK + B4B TRANSPORT)" : "FAIL";
        }
    }

    g_driver = nullptr;
    driver->Release();
    factory->Release();
    const HRESULT unload_hr = can_unload();
    std::printf("DllCanUnloadNow hr=0x%08lX\n", static_cast<unsigned long>(unload_hr));
    pass = pass && unload_hr == S_OK;
    if (unload_hr != S_OK) label = "FAIL";
    FreeLibrary(module);
    std::printf("STAGE B4C TIME INFO RESULT: %s\n", label);
    return pass ? 0 : 8;
}

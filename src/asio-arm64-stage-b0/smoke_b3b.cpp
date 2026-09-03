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
#error Stage B3B smoke must be built for native Windows ARM64, not ARM64EC.
#endif

using DllGetClassObjectFn = HRESULT (STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*);
using DllCanUnloadNowFn = HRESULT (STDAPICALLTYPE*)();

namespace {

constexpr long kFrames = 512;
constexpr double kSampleRate = 48000.0;
constexpr double kToneHz = 440.0;
constexpr double kTonePeak = 1200.0;
constexpr double kTwoPi = 6.283185307179586476925286766559;

ASIOBufferInfo* g_buffer_infos = nullptr;
ULONG g_callback_count = 0;
ULONG g_index_errors = 0;
ULONG g_direct_process_errors = 0;
ULONG g_host_sample_writes = 0;
long g_last_index = -1;
double g_phase = 0.0;

bool contains_text(const char* text, const char* needle) {
    return text && needle && std::strstr(text, needle) != nullptr;
}

void read_error(IASIO* driver, char (&buffer)[124]) {
    buffer[0] = '\0';
    driver->getErrorMessage(buffer);
}

void buffer_switch(long doubleBufferIndex, ASIOBool directProcess) {
    if (doubleBufferIndex != 0 && doubleBufferIndex != 1) {
        ++g_index_errors;
        return;
    }
    if (g_last_index >= 0 && g_last_index == doubleBufferIndex) ++g_index_errors;
    g_last_index = doubleBufferIndex;
    if (directProcess != ASIOFalse) ++g_direct_process_errors;
    ++g_callback_count;

    if (g_buffer_infos) {
        auto* left = static_cast<std::int16_t*>(g_buffer_infos[0].buffers[doubleBufferIndex]);
        auto* right = static_cast<std::int16_t*>(g_buffer_infos[1].buffers[doubleBufferIndex]);
        if (left && right) {
            const double phase_step = kTwoPi * kToneHz / kSampleRate;
            for (long frame = 0; frame < kFrames; ++frame) {
                const auto sample = static_cast<std::int16_t>(std::sin(g_phase) * kTonePeak);
                left[frame] = sample;
                right[frame] = sample;
                g_host_sample_writes += 2;
                g_phase += phase_step;
                if (g_phase >= kTwoPi) g_phase -= kTwoPi;
            }
        }
    }

    std::printf(
        "B3B bufferSwitch callback=%lu index=%ld directProcess=%ld tone=440Hz peak=1200\n",
        g_callback_count, doubleBufferIndex, directProcess);
}

void sample_rate_did_change(ASIOSampleRate rate) {
    std::printf("B3B unexpected sampleRateDidChange rate=%.1f\n", rate);
}

long asio_message(long selector, long value, void* message, double* opt) {
    (void)selector;
    (void)value;
    (void)message;
    (void)opt;
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

} // namespace

int main() {
    std::printf("Sound Blaster X4 ARM64 ASIO Stage B3B host-to-WaveRT sample transfer smoke\n");
    std::printf("SAFETY: registry-free; BUSY gates preserved; mapped DMA copy enabled; SETWRITEPACKET disabled.\n");
    std::printf("AUDIO: low-level 440 Hz stereo test tone, peak=1200/32767, about 20 callbacks. Set speaker/headphone volume low first.\n");
    std::printf("ABI sizeof(ASIOBufferInfo)=%zu align=%zu sizeof(ASIOCallbacks)=%zu align=%zu\n",
                sizeof(ASIOBufferInfo), alignof(ASIOBufferInfo),
                sizeof(ASIOCallbacks), alignof(ASIOCallbacks));

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

    const ASIOBool init_ok = driver->init(nullptr);
    char init_error[124]{};
    read_error(driver, init_error);
    std::printf("init=%ld\n", init_ok);
    std::printf("initMessage=%s\n", init_error);

    char driver_name[32]{};
    driver->getDriverName(driver_name);
    std::printf("driverName=%s\n", driver_name);
    std::printf("driverVersion=%ld\n", driver->getDriverVersion());

    bool pass = false;
    const char* result_label = "FAIL";

    if (init_ok != ASIOTrue) {
        const bool safely_busy =
            contains_text(init_error, "BUSY") &&
            contains_text(init_error, "KsCreatePin SKIPPED");
        std::printf("createBuffers/start/stop=SKIPPED because init did not report FREE\n");
        pass = safely_busy;
        result_label = safely_busy ? "PASS (BUSY SAFELY BLOCKED AT INIT)" : "FAIL";
    } else {
        ASIOBufferInfo infos[2]{};
        infos[0].isInput = ASIOFalse;
        infos[0].channelNum = 0;
        infos[1].isInput = ASIOFalse;
        infos[1].channelNum = 1;

        ASIOCallbacks callbacks{};
        callbacks.bufferSwitch = &buffer_switch;
        callbacks.sampleRateDidChange = &sample_rate_did_change;
        callbacks.asioMessage = &asio_message;
        callbacks.bufferSwitchTimeInfo = nullptr;

        const ASIOError create_hr = driver->createBuffers(infos, 2, kFrames, &callbacks);
        char create_error[124]{};
        read_error(driver, create_error);
        std::printf("createBuffers=%ld\n", create_hr);
        std::printf("createMessage=%s\n", create_error);

        if (create_hr != ASE_OK) {
            const bool safely_busy =
                contains_text(create_error, "PRE-PIN gate BUSY") &&
                contains_text(create_error, "KsCreatePin SKIPPED");
            std::printf("start/stop=SKIPPED because WaveRT preparation did not succeed\n");
            driver->disposeBuffers();
            pass = safely_busy;
            result_label = safely_busy ? "PASS (RACE BUSY SAFELY BLOCKED PRE-PIN)" : "FAIL";
        } else {
            const bool pointers_ok = distinct_non_null_buffers(infos);
            std::printf("ASIO buffers ch0={%p,%p} ch1={%p,%p} distinctNonNull=%s\n",
                        infos[0].buffers[0], infos[0].buffers[1],
                        infos[1].buffers[0], infos[1].buffers[1],
                        pointers_ok ? "YES" : "NO");

            g_buffer_infos = infos;
            g_callback_count = 0;
            g_index_errors = 0;
            g_direct_process_errors = 0;
            g_host_sample_writes = 0;
            g_last_index = -1;
            g_phase = 0.0;

            const ASIOError start_hr = driver->start();
            char start_error[124]{};
            read_error(driver, start_error);
            std::printf("start=%ld\n", start_hr);
            std::printf("startMessage=%s\n", start_error);
            std::printf("callbackStats count=%lu indexErrors=%lu directProcessErrors=%lu hostSampleWrites=%lu\n",
                        g_callback_count, g_index_errors,
                        g_direct_process_errors, g_host_sample_writes);

            const ASIOError stop_hr = driver->stop();
            char stop_error[124]{};
            read_error(driver, stop_error);
            std::printf("stop=%ld\n", stop_hr);
            std::printf("stopMessage=%s\n", stop_error);

            const ASIOError dispose_hr = driver->disposeBuffers();
            g_buffer_infos = nullptr;
            char dispose_error[124]{};
            read_error(driver, dispose_error);
            std::printf("disposeBuffers=%ld\n", dispose_hr);
            std::printf("disposeMessage=%s\n", dispose_error);

            const bool pointers_cleared =
                infos[0].buffers[0] == nullptr && infos[0].buffers[1] == nullptr &&
                infos[1].buffers[0] == nullptr && infos[1].buffers[1] == nullptr;
            std::printf("dispose cleared ASIO buffer pointers=%s\n", pointers_cleared ? "YES" : "NO");

            pass =
                pointers_ok &&
                start_hr == ASE_OK &&
                contains_text(start_error, "dmaWrites=20") &&
                contains_text(start_error, "dmaFrames=10240") &&
                g_callback_count == 20 &&
                g_index_errors == 0 &&
                g_direct_process_errors == 0 &&
                g_host_sample_writes == 20480 &&
                stop_hr == ASE_OK &&
                dispose_hr == ASE_OK &&
                pointers_cleared;
            result_label = pass ? "PASS (HOST PCM COPIED TO WAVERT DMA)" : "FAIL";
        }
    }

    driver->Release();
    factory->Release();

    const HRESULT unload_hr = can_unload();
    std::printf("DllCanUnloadNow hr=0x%08lX\n", static_cast<unsigned long>(unload_hr));
    pass = pass && unload_hr == S_OK;
    if (unload_hr != S_OK) result_label = "FAIL";

    FreeLibrary(module);
    std::printf("STAGE B3B DMA COPY RESULT: %s\n", result_label);
    return pass ? 0 : 8;
}

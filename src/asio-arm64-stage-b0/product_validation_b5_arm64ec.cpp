#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "asio_callback_compat.h"
#include "b5_identity.h"

#if !defined(_M_ARM64EC)
#error B5 product validation host must be compiled as ARM64EC.
#endif

namespace {

struct ActiveBuffers {
    ASIOBufferInfo* infos = nullptr;
    long count = 0;
    long frames = 0;
    volatile LONG callbacks = 0;
    volatile LONG input_nonzero_samples = 0;
};

ActiveBuffers g_active{};

void buffer_switch(long index, ASIOBool) {
    if (index < 0 || index > 1 || !g_active.infos) return;
    InterlockedIncrement(&g_active.callbacks);
    for (long i = 0; i < g_active.count; ++i) {
        auto& info = g_active.infos[i];
        auto* data = static_cast<std::uint8_t*>(info.buffers[index]);
        if (!data) continue;
        if (info.isInput != ASIOFalse) {
            for (long frame = 0; frame < g_active.frames; ++frame) {
                const auto* s = data + frame * 3;
                if (s[0] || s[1] || s[2]) InterlockedIncrement(&g_active.input_nonzero_samples);
            }
        } else {
            ZeroMemory(data, static_cast<SIZE_T>(g_active.frames) * 3u);
        }
    }
}

void sample_rate_changed(ASIOSampleRate rate) {
    std::printf("validation callback sampleRateDidChange=%.0f\n", rate);
}

long asio_message(long selector, long value, void*, double*) {
    (void)value;
    if (selector == kAsioSelectorSupported) return 1;
    if (selector == kAsioEngineVersion) return 2;
    if (selector == kAsioSupportsTimeInfo) return 1;
    return 0;
}

ASIOTime* buffer_switch_time_info(ASIOTime* params, long index, ASIOBool direct) {
    buffer_switch(index, direct);
    return params;
}

void get_error(IASIO* asio, char (&message)[124]) {
    ZeroMemory(message, sizeof(message));
    if (asio) asio->getErrorMessage(message);
}

struct TestCase {
    const char* name;
    ASIOSampleRate rate;
    long frames;
    bool duplex;
    int cycles;
    DWORD run_ms;
};

int run_case(const TestCase& tc) {
    std::printf("=== B5 VALIDATION CASE %s rate=%.0f frames=%ld duplex=%d cycles=%d ===\n",
                tc.name, tc.rate, tc.frames, tc.duplex ? 1 : 0, tc.cycles);

    for (int cycle = 1; cycle <= tc.cycles; ++cycle) {
        IASIO* asio = nullptr;
        const HRESULT hr = CoCreateInstance(
            CLSID_X4_ARM64_ASIO_B5, nullptr, CLSCTX_INPROC_SERVER,
            CLSID_X4_ARM64_ASIO_B5, reinterpret_cast<void**>(&asio));
        if (FAILED(hr) || !asio) {
            std::printf("case=%s cycle=%d CoCreateInstance=0x%08lX FAIL\n",
                        tc.name, cycle, static_cast<unsigned long>(hr));
            return 20;
        }

        if (asio->init(nullptr) != ASIOTrue) {
            char message[124]{};
            get_error(asio, message);
            std::printf("case=%s cycle=%d init=FAIL message=%s\n", tc.name, cycle, message);
            asio->Release();
            if (std::strstr(message, "BUSY") || std::strstr(message, "INDETERMINATE")) return 10;
            return 21;
        }

        const ASIOError rate_result = asio->setSampleRate(tc.rate);
        if (rate_result != ASE_OK) {
            char message[124]{};
            get_error(asio, message);
            std::printf("case=%s cycle=%d setSampleRate=%ld message=%s\n",
                        tc.name, cycle, rate_result, message);
            asio->Release();
            return 22;
        }

        long min_size = 0;
        long max_size = 0;
        long preferred_size = 0;
        long granularity = 0;
        const ASIOError buffer_contract = asio->getBufferSize(
            &min_size, &max_size, &preferred_size, &granularity);
        const long expected_min = tc.rate == 192000.0 ? 384 : 96;
        const long expected_preferred = tc.rate == 192000.0 ? 384 : 240;
        if (buffer_contract != ASE_OK ||
            min_size != expected_min || max_size != 4800 ||
            preferred_size != expected_preferred || granularity != 48) {
            std::printf(
                "case=%s cycle=%d bufferContract result=%ld min=%ld max=%ld preferred=%ld granularity=%ld expectedMin=%ld expectedPreferred=%ld FAIL\n",
                tc.name, cycle, buffer_contract, min_size, max_size,
                preferred_size, granularity, expected_min, expected_preferred);
            asio->Release();
            return 29;
        }
        std::printf(
            "case=%s cycle=%d bufferContract min=%ld max=%ld preferred=%ld granularity=%ld PASS\n",
            tc.name, cycle, min_size, max_size, preferred_size, granularity);

        long inputs = 0;
        long outputs = 0;
        if (asio->getChannels(&inputs, &outputs) != ASE_OK || outputs != 2 ||
            (tc.duplex && inputs != 2) || (!tc.duplex && tc.rate == 192000.0 && inputs != 0)) {
            std::printf("case=%s cycle=%d channels inputs=%ld outputs=%ld FAIL\n",
                        tc.name, cycle, inputs, outputs);
            asio->Release();
            return 23;
        }

        ASIOChannelInfo channel_info{};
        channel_info.channel = 0;
        channel_info.isInput = ASIOFalse;
        if (asio->getChannelInfo(&channel_info) != ASE_OK || channel_info.type != 17) {
            std::printf("case=%s cycle=%d output sampleType=%ld FAIL\n",
                        tc.name, cycle, channel_info.type);
            asio->Release();
            return 24;
        }
        if (tc.duplex) {
            channel_info = {};
            channel_info.channel = 0;
            channel_info.isInput = ASIOTrue;
            if (asio->getChannelInfo(&channel_info) != ASE_OK || channel_info.type != 17) {
                std::printf("case=%s cycle=%d input sampleType=%ld FAIL\n",
                            tc.name, cycle, channel_info.type);
                asio->Release();
                return 25;
            }
        }

        std::array<ASIOBufferInfo, 4> infos{};
        long count = 0;
        if (tc.duplex) {
            infos[count].isInput = ASIOTrue;
            infos[count++].channelNum = 0;
            infos[count].isInput = ASIOTrue;
            infos[count++].channelNum = 1;
        }
        infos[count].isInput = ASIOFalse;
        infos[count++].channelNum = 0;
        infos[count].isInput = ASIOFalse;
        infos[count++].channelNum = 1;

        ASIOCallbacks callbacks{};
        callbacks.bufferSwitch = &buffer_switch;
        callbacks.sampleRateDidChange = &sample_rate_changed;
        callbacks.asioMessage = &asio_message;
        callbacks.bufferSwitchTimeInfo = &buffer_switch_time_info;

        g_active.infos = infos.data();
        g_active.count = count;
        g_active.frames = tc.frames;
        InterlockedExchange(&g_active.callbacks, 0);
        InterlockedExchange(&g_active.input_nonzero_samples, 0);

        const ASIOError create_result = asio->createBuffers(infos.data(), count, tc.frames, &callbacks);
        if (create_result != ASE_OK) {
            char message[124]{};
            get_error(asio, message);
            std::printf("case=%s cycle=%d createBuffers=%ld FAIL message=%s\n",
                        tc.name, cycle, create_result, message);
            g_active = {};
            asio->Release();
            return 26;
        }

        long input_latency = 0;
        long output_latency = 0;
        asio->getLatencies(&input_latency, &output_latency);

        const ASIOError start_result = asio->start();
        if (start_result != ASE_OK) {
            char message[124]{};
            get_error(asio, message);
            std::printf("case=%s cycle=%d start=%ld FAIL message=%s\n",
                        tc.name, cycle, start_result, message);
            asio->disposeBuffers();
            g_active = {};
            asio->Release();
            return 27;
        }

        Sleep(tc.run_ms);
        const ASIOError stop_result = asio->stop();
        char stop_message[124]{};
        get_error(asio, stop_message);
        const LONG callback_count = InterlockedCompareExchange(&g_active.callbacks, 0, 0);
        const LONG input_nonzero = InterlockedCompareExchange(&g_active.input_nonzero_samples, 0, 0);

        std::printf(
            "case=%s cycle=%d callbacks=%ld inputNonzeroSamples=%ld latencyIn=%ld latencyOut=%ld stop=%ld message=%s\n",
            tc.name, cycle, callback_count, input_nonzero,
            input_latency, output_latency, stop_result, stop_message);

        const ASIOError dispose_result = asio->disposeBuffers();
        g_active = {};
        asio->Release();

        if (callback_count <= 0 || stop_result != ASE_OK || dispose_result != ASE_OK) {
            return 28;
        }
    }
    return 0;
}

} // namespace

int main() {
    const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(co)) {
        std::printf("B5 VALIDATION CoInitializeEx=0x%08lX FAIL\n", static_cast<unsigned long>(co));
        return 2;
    }

    const TestCase cases[] = {
        {"preferred-48-output", 48000.0, 240, false, 3, 700},
        {"preferred-48-duplex", 48000.0, 240, true, 2, 700},
        {"preferred-96-duplex", 96000.0, 240, true, 2, 700},
        {"preferred-192-output", 192000.0, 384, false, 2, 700},
        {"minimum-48-output", 48000.0, 96, false, 1, 500},
        {"maximum-48-output", 48000.0, 4800, false, 1, 900},
        {"b4d-512-compat", 48000.0, 512, false, 1, 700},
    };

    int result = 0;
    for (const auto& tc : cases) {
        result = run_case(tc);
        if (result != 0) break;
        Sleep(150);
    }

    CoUninitialize();
    if (result == 10) {
        std::puts("B5 PRODUCT VALIDATION RESULT: BUSY_BLOCKED");
        return 10;
    }
    std::printf("B5 PRODUCT VALIDATION RESULT: %s code=%d\n",
                result == 0 ? "PASS" : "FAIL", result);
    return result;
}

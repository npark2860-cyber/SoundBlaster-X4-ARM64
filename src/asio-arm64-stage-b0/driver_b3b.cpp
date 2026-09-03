#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <new>

#include "asio_callback_compat.h"
#include "preflight.h"
#include "wavert_engine.h"

#if !defined(_M_ARM64) || defined(_M_ARM64EC)
#error Stage B3B must be built for native Windows ARM64, not ARM64EC.
#endif

namespace {

constexpr long kHostChannels = 2;
constexpr long kHostBufferFrames = 512;
constexpr ULONG kNotificationCount = 2;

volatile LONG g_object_count = 0;
volatile LONG g_lock_count = 0;
HMODULE g_module = nullptr;

bool guid_equal(REFGUID a, REFGUID b) {
    return !!InlineIsEqualGUID(a, b);
}

class X4AsioDriver final : public IASIO {
public:
    X4AsioDriver() { InterlockedIncrement(&g_object_count); }

    ~X4AsioDriver() {
        release_host_buffers();
        engine_.dispose();
        InterlockedDecrement(&g_object_count);
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (guid_equal(riid, IID_IUnknown) || guid_equal(riid, CLSID_X4_ARM64_ASIO)) {
            *ppv = static_cast<IASIO*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(InterlockedIncrement(&ref_count_));
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const LONG count = InterlockedDecrement(&ref_count_);
        if (count == 0) {
            delete this;
            return 0;
        }
        return static_cast<ULONG>(count);
    }

    ASIOBool init(void* sysHandle) override {
        (void)sysHandle;
        release_host_buffers();
        engine_.dispose();
        initialized_free_ = false;

        const X4InstancePreflightResult preflight = run_x4_instance_preflight();
        if (!preflight.device_found) {
            strcpy_s(last_error_, "B3B init FAILED: X4 msft_wave not found");
            return ASIOFalse;
        }
        if (!preflight.filter_opened) {
            sprintf_s(last_error_, sizeof(last_error_),
                      "B3B init FAILED: filter open Win32=%lu", preflight.open_error);
            return ASIOFalse;
        }
        if (!preflight.local_ok || !preflight.global_ok) {
            sprintf_s(last_error_, sizeof(last_error_),
                      "B3B init INDETERMINATE: C ok=%d G ok=%d; KsCreatePin SKIPPED",
                      preflight.local_ok ? 1 : 0, preflight.global_ok ? 1 : 0);
            return ASIOFalse;
        }

        const bool busy =
            preflight.local_current >= preflight.local_possible ||
            preflight.global_current >= preflight.global_possible;
        if (busy) {
            sprintf_s(last_error_, sizeof(last_error_),
                      "B3B init BUSY: C %lu/%lu G %lu/%lu; KsCreatePin SKIPPED",
                      preflight.local_current, preflight.local_possible,
                      preflight.global_current, preflight.global_possible);
            return ASIOFalse;
        }

        initialized_free_ = true;
        sprintf_s(last_error_, sizeof(last_error_),
                  "B3B init FREE: C %lu/%lu G %lu/%lu; buffers not created",
                  preflight.local_current, preflight.local_possible,
                  preflight.global_current, preflight.global_possible);
        return ASIOTrue;
    }

    void getDriverName(char* name) override {
        if (name) strcpy_s(name, 32, kX4AsioDriverName);
    }

    long getDriverVersion() override { return 104; }

    void getErrorMessage(char* string) override {
        if (string) strcpy_s(string, 124, last_error_);
    }

    ASIOError start() override {
        if (!initialized_free_ || !buffers_created_ || !engine_.prepared()) {
            strcpy_s(last_error_, "B3B start rejected: ASIO/WaveRT buffers not prepared");
            return ASE_InvalidMode;
        }

        callback_count_ = 0;
        callback_index_errors_ = 0;
        dma_copy_errors_ = 0;
        last_callback_index_ = -1;

        const bool ok = engine_.start_and_observe(&X4AsioDriver::notification_observer, this);
        if (!ok) {
            strcpy_s(last_error_, engine_.last_message());
            return ASE_HWMalfunction;
        }

        const auto& stats = engine_.stats();
        const bool run_ok =
            callback_count_ == 20 &&
            callback_index_errors_ == 0 &&
            dma_copy_errors_ == 0 &&
            stats.notifications == 20 &&
            stats.packet_discontinuities == 0 &&
            stats.position_regressions == 0 &&
            stats.hardware_buffer_writes == 20 &&
            stats.dma_frames_copied == 20 * kHostBufferFrames &&
            stats.dma_nonzero_samples > 0;

        sprintf_s(last_error_, sizeof(last_error_),
                  "B3B RUN notif=%lu cb=%lu dmaWrites=%lu dmaFrames=%lu nonzero=%lu",
                  stats.notifications, callback_count_, stats.hardware_buffer_writes,
                  stats.dma_frames_copied, stats.dma_nonzero_samples);
        return run_ok ? ASE_OK : ASE_HWMalfunction;
    }

    ASIOError stop() override {
        if (!engine_.prepared()) return ASE_OK;
        const bool ok = engine_.stop();
        if (ok) {
            strcpy_s(last_error_, "B3B stop OK: RUN->PAUSE->ACQUIRE->STOP");
            return ASE_OK;
        }
        strcpy_s(last_error_, engine_.last_message());
        return ASE_HWMalfunction;
    }

    ASIOError getChannels(long* numInputChannels, long* numOutputChannels) override {
        if (!numInputChannels || !numOutputChannels) return ASE_InvalidParameter;
        *numInputChannels = 0;
        *numOutputChannels = kHostChannels;
        return ASE_OK;
    }

    ASIOError getLatencies(long* inputLatency, long* outputLatency) override {
        if (!inputLatency || !outputLatency) return ASE_InvalidParameter;
        *inputLatency = 0;
        *outputLatency = kHostBufferFrames * 2;
        return ASE_OK;
    }

    ASIOError getBufferSize(long* minSize, long* maxSize, long* preferredSize, long* granularity) override {
        if (!minSize || !maxSize || !preferredSize || !granularity) return ASE_InvalidParameter;
        *minSize = kHostBufferFrames;
        *maxSize = kHostBufferFrames;
        *preferredSize = kHostBufferFrames;
        *granularity = 0;
        return ASE_OK;
    }

    ASIOError canSampleRate(ASIOSampleRate sampleRate) override {
        return sampleRate == 48000.0 ? ASE_OK : ASE_NoClock;
    }

    ASIOError getSampleRate(ASIOSampleRate* sampleRate) override {
        if (!sampleRate) return ASE_InvalidParameter;
        *sampleRate = 48000.0;
        return ASE_OK;
    }

    ASIOError setSampleRate(ASIOSampleRate sampleRate) override {
        return sampleRate == 48000.0 ? ASE_OK : ASE_NoClock;
    }

    ASIOError getClockSources(ASIOClockSource* clocks, long* numSources) override {
        (void)clocks;
        if (!numSources) return ASE_InvalidParameter;
        *numSources = 0;
        return ASE_NotPresent;
    }

    ASIOError setClockSource(long reference) override {
        (void)reference;
        return ASE_NotPresent;
    }

    ASIOError getSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp) override {
        (void)sPos;
        (void)tStamp;
        return ASE_NotPresent;
    }

    ASIOError getChannelInfo(ASIOChannelInfo* info) override {
        (void)info;
        return ASE_NotPresent;
    }

    ASIOError createBuffers(
        ASIOBufferInfo* bufferInfos,
        long numChannels,
        long bufferSize,
        ASIOCallbacks* callbacks) override {

        if (!initialized_free_) {
            strcpy_s(last_error_, "B3B createBuffers rejected: init not FREE");
            return ASE_InvalidMode;
        }
        if (!bufferInfos || !callbacks || !callbacks->bufferSwitch) {
            strcpy_s(last_error_, "B3B createBuffers requires bufferInfos + bufferSwitch");
            return ASE_InvalidParameter;
        }
        if (numChannels != kHostChannels || bufferSize != kHostBufferFrames) {
            strcpy_s(last_error_, "B3B createBuffers requires 2 outputs x 512 frames");
            return ASE_InvalidParameter;
        }
        if (buffers_created_ || engine_.prepared()) {
            strcpy_s(last_error_, "B3B createBuffers rejected: already created");
            return ASE_InvalidMode;
        }

        bool seen_channel[2] = {false, false};
        for (long i = 0; i < numChannels; ++i) {
            const long channel = bufferInfos[i].channelNum;
            if (bufferInfos[i].isInput != ASIOFalse ||
                channel < 0 || channel >= kHostChannels || seen_channel[channel]) {
                strcpy_s(last_error_, "B3B requires unique output channels 0 and 1");
                return ASE_InvalidParameter;
            }
            seen_channel[channel] = true;
        }

        const X4WaveRtPrepareResult prepare_result = engine_.prepare();
        if (prepare_result != X4WaveRtPrepareResult::Ready) {
            strcpy_s(last_error_, engine_.last_message());
            return ASE_HWMalfunction;
        }

        ZeroMemory(host_buffers_, sizeof(host_buffers_));
        callbacks_ = *callbacks;
        host_buffer_infos_ = bufferInfos;
        host_buffer_info_count_ = numChannels;

        for (long i = 0; i < numChannels; ++i) {
            const long channel = bufferInfos[i].channelNum;
            bufferInfos[i].buffers[0] = host_buffers_[channel][0];
            bufferInfos[i].buffers[1] = host_buffers_[channel][1];
        }

        buffers_created_ = true;
        strcpy_s(last_error_, "B3B buffers ready: planar host -> interleaved WaveRT copy enabled");
        return ASE_OK;
    }

    ASIOError disposeBuffers() override {
        engine_.dispose();
        release_host_buffers();
        strcpy_s(last_error_, "B3B disposeBuffers OK: host + WaveRT buffers detached");
        return ASE_OK;
    }

    ASIOError controlPanel() override { return ASE_NotPresent; }

    ASIOError future(long selector, void* opt) override {
        (void)selector;
        (void)opt;
        return ASE_NotPresent;
    }

    ASIOError outputReady() override { return ASE_NotPresent; }

private:
    static bool notification_observer(
        void* context,
        ULONG zero_based_notification_index,
        ULONG packet_count) {

        auto* self = static_cast<X4AsioDriver*>(context);
        if (!self || !self->buffers_created_ || !self->callbacks_.bufferSwitch) return false;

        const ULONG write_packet = packet_count + 1;
        const long buffer_index = static_cast<long>(write_packet % kNotificationCount);

        if (self->last_callback_index_ >= 0 && self->last_callback_index_ == buffer_index) {
            ++self->callback_index_errors_;
        }
        self->last_callback_index_ = buffer_index;
        ++self->callback_count_;

        self->callbacks_.bufferSwitch(buffer_index, ASIOFalse);

        const bool copied = self->engine_.write_interleaved_packet(
            write_packet,
            self->host_buffers_[0][buffer_index],
            self->host_buffers_[1][buffer_index],
            kHostBufferFrames);
        if (!copied) ++self->dma_copy_errors_;

        std::printf(
            "B3B callback=%lu notificationIndex=%lu packet=%lu writePacket=%lu slot=%ld copy=%s\n",
            self->callback_count_, zero_based_notification_index, packet_count,
            write_packet, buffer_index, copied ? "OK" : "FAIL");
        return copied;
    }

    void release_host_buffers() {
        if (host_buffer_infos_) {
            for (long i = 0; i < host_buffer_info_count_; ++i) {
                host_buffer_infos_[i].buffers[0] = nullptr;
                host_buffer_infos_[i].buffers[1] = nullptr;
            }
        }
        host_buffer_infos_ = nullptr;
        host_buffer_info_count_ = 0;
        callbacks_ = {};
        buffers_created_ = false;
        callback_count_ = 0;
        callback_index_errors_ = 0;
        dma_copy_errors_ = 0;
        last_callback_index_ = -1;
        ZeroMemory(host_buffers_, sizeof(host_buffers_));
    }

    volatile LONG ref_count_ = 1;
    bool initialized_free_ = false;
    bool buffers_created_ = false;
    X4WaveRtEngine engine_{};
    ASIOCallbacks callbacks_{};
    ASIOBufferInfo* host_buffer_infos_ = nullptr;
    long host_buffer_info_count_ = 0;
    alignas(64) std::int16_t host_buffers_[2][2][kHostBufferFrames]{};
    ULONG callback_count_ = 0;
    ULONG callback_index_errors_ = 0;
    ULONG dma_copy_errors_ = 0;
    long last_callback_index_ = -1;
    char last_error_[124] = "Stage B3B host-to-WaveRT transfer not initialized";
};

class X4ClassFactory final : public IClassFactory {
public:
    X4ClassFactory() { InterlockedIncrement(&g_object_count); }
    ~X4ClassFactory() { InterlockedDecrement(&g_object_count); }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (guid_equal(riid, IID_IUnknown) || guid_equal(riid, IID_IClassFactory)) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(InterlockedIncrement(&ref_count_));
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const LONG count = InterlockedDecrement(&ref_count_);
        if (count == 0) {
            delete this;
            return 0;
        }
        return static_cast<ULONG>(count);
    }

    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;
        auto* object = new (std::nothrow) X4AsioDriver();
        if (!object) return E_OUTOFMEMORY;
        const HRESULT hr = object->QueryInterface(riid, ppv);
        object->Release();
        return hr;
    }

    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override {
        if (lock) InterlockedIncrement(&g_lock_count);
        else InterlockedDecrement(&g_lock_count);
        return S_OK;
    }

private:
    volatile LONG ref_count_ = 1;
};

HRESULT set_registry_string(HKEY root, const wchar_t* path, const wchar_t* name, const wchar_t* value) {
    HKEY key = nullptr;
    const LONG open_result = RegCreateKeyExW(
        root, path, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE,
        nullptr, &key, nullptr);
    if (open_result != ERROR_SUCCESS) return HRESULT_FROM_WIN32(open_result);
    const DWORD bytes = static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t));
    const LONG set_result = RegSetValueExW(
        key, name, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(value), bytes);
    RegCloseKey(key);
    return HRESULT_FROM_WIN32(set_result);
}

HRESULT register_server() {
    if (!g_module) return E_UNEXPECTED;
    wchar_t module_path[MAX_PATH]{};
    if (!GetModuleFileNameW(g_module, module_path, MAX_PATH)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    wchar_t clsid_key[160]{};
    wchar_t inproc_key[192]{};
    wchar_t asio_key[256]{};
    swprintf_s(clsid_key, L"SOFTWARE\\Classes\\CLSID\\%ls", kX4AsioClsidString);
    swprintf_s(inproc_key, L"%ls\\InprocServer32", clsid_key);
    swprintf_s(asio_key, L"SOFTWARE\\ASIO\\%ls", kX4AsioRegistryName);

    HRESULT hr = set_registry_string(HKEY_LOCAL_MACHINE, clsid_key, nullptr, kX4AsioDescription);
    if (FAILED(hr)) return hr;
    hr = set_registry_string(HKEY_LOCAL_MACHINE, inproc_key, nullptr, module_path);
    if (FAILED(hr)) return hr;
    hr = set_registry_string(HKEY_LOCAL_MACHINE, inproc_key, L"ThreadingModel", L"Apartment");
    if (FAILED(hr)) return hr;
    hr = set_registry_string(HKEY_LOCAL_MACHINE, asio_key, L"CLSID", kX4AsioClsidString);
    if (FAILED(hr)) return hr;
    return set_registry_string(HKEY_LOCAL_MACHINE, asio_key, L"Description", kX4AsioDescription);
}

HRESULT unregister_server() {
    wchar_t clsid_key[160]{};
    wchar_t asio_key[256]{};
    swprintf_s(clsid_key, L"SOFTWARE\\Classes\\CLSID\\%ls", kX4AsioClsidString);
    swprintf_s(asio_key, L"SOFTWARE\\ASIO\\%ls", kX4AsioRegistryName);

    const LONG asio_result = RegDeleteTreeW(HKEY_LOCAL_MACHINE, asio_key);
    const LONG clsid_result = RegDeleteTreeW(HKEY_LOCAL_MACHINE, clsid_key);
    if (asio_result != ERROR_SUCCESS && asio_result != ERROR_FILE_NOT_FOUND) {
        return HRESULT_FROM_WIN32(asio_result);
    }
    if (clsid_result != ERROR_SUCCESS && clsid_result != ERROR_FILE_NOT_FOUND) {
        return HRESULT_FROM_WIN32(clsid_result);
    }
    return S_OK;
}

} // namespace

extern "C" HRESULT __stdcall DllCanUnloadNow() {
    return (g_object_count == 0 && g_lock_count == 0) ? S_OK : S_FALSE;
}

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (!guid_equal(rclsid, CLSID_X4_ARM64_ASIO)) return CLASS_E_CLASSNOTAVAILABLE;
    auto* factory = new (std::nothrow) X4ClassFactory();
    if (!factory) return E_OUTOFMEMORY;
    const HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

extern "C" HRESULT __stdcall DllRegisterServer() { return register_server(); }
extern "C" HRESULT __stdcall DllUnregisterServer() { return unregister_server(); }

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

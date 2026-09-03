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
#include "preflight.h"
#include "wavert_engine.h"

#if !defined(_M_ARM64) || defined(_M_ARM64EC)
#error Stage B4C must be built for native Windows ARM64, not ARM64EC.
#endif

namespace {

constexpr long kHostChannels = 2;
constexpr long kHostBufferFrames = 512;
constexpr ULONG kNotificationCount = 2;
constexpr DWORD kWorkerJoinTimeoutMs = 2000;
constexpr ASIOSampleRate kSampleRate = 48000.0;

volatile LONG g_object_count = 0;
volatile LONG g_lock_count = 0;
HMODULE g_module = nullptr;

bool guid_equal(REFGUID a, REFGUID b) {
    return !!InlineIsEqualGUID(a, b);
}

ASIOTimeStamp asio_system_time_ns() {
    return static_cast<ASIOTimeStamp>(timeGetTime()) * 1000000LL;
}

class X4AsioDriver final : public IASIO {
public:
    X4AsioDriver() { InterlockedIncrement(&g_object_count); }

    ~X4AsioDriver() {
        force_join_worker();
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

        if (worker_thread_ || engine_.running()) {
            const ASIOError stop_result = stop();
            if (stop_result != ASE_OK && worker_thread_) {
                strcpy_s(last_error_, "B4C init rejected: existing worker could not be joined safely");
                return ASIOFalse;
            }
        }

        release_host_buffers();
        engine_.dispose();
        initialized_free_ = false;
        InterlockedExchange64(&sample_position_, 0);
        InterlockedExchange64(&sample_timestamp_ns_, 0);

        const X4InstancePreflightResult preflight = run_x4_instance_preflight();
        if (!preflight.device_found) {
            strcpy_s(last_error_, "B4C init FAILED: X4 msft_wave not found");
            return ASIOFalse;
        }
        if (!preflight.filter_opened) {
            sprintf_s(last_error_, sizeof(last_error_),
                      "B4C init FAILED: filter open Win32=%lu", preflight.open_error);
            return ASIOFalse;
        }
        if (!preflight.local_ok || !preflight.global_ok) {
            sprintf_s(last_error_, sizeof(last_error_),
                      "B4C init INDETERMINATE: C ok=%d G ok=%d; KsCreatePin SKIPPED",
                      preflight.local_ok ? 1 : 0, preflight.global_ok ? 1 : 0);
            return ASIOFalse;
        }

        const bool busy =
            preflight.local_current >= preflight.local_possible ||
            preflight.global_current >= preflight.global_possible;
        if (busy) {
            sprintf_s(last_error_, sizeof(last_error_),
                      "B4C init BUSY: C %lu/%lu G %lu/%lu; KsCreatePin SKIPPED",
                      preflight.local_current, preflight.local_possible,
                      preflight.global_current, preflight.global_possible);
            return ASIOFalse;
        }

        initialized_free_ = true;
        sprintf_s(last_error_, sizeof(last_error_),
                  "B4C init FREE: C %lu/%lu G %lu/%lu; ASIO2 time-info available",
                  preflight.local_current, preflight.local_possible,
                  preflight.global_current, preflight.global_possible);
        return ASIOTrue;
    }

    void getDriverName(char* name) override {
        if (name) strcpy_s(name, 32, kX4AsioDriverName);
    }

    long getDriverVersion() override { return 107; }

    void getErrorMessage(char* string) override {
        if (string) strcpy_s(string, 124, last_error_);
    }

    ASIOError start() override {
        if (!initialized_free_ || !buffers_created_ || !engine_.prepared()) {
            strcpy_s(last_error_, "B4C start rejected: ASIO/WaveRT buffers not prepared");
            return ASE_InvalidMode;
        }
        if (worker_thread_ || engine_.running()) {
            strcpy_s(last_error_, "B4C start rejected: worker/RUN already active");
            return ASE_InvalidMode;
        }

        InterlockedExchange(&callback_count_, 0);
        InterlockedExchange(&callback_index_errors_, 0);
        InterlockedExchange(&dma_copy_errors_, 0);
        InterlockedExchange(&worker_failed_, 0);
        InterlockedExchange(&worker_running_, 0);
        InterlockedExchange64(&sample_position_, 0);
        InterlockedExchange64(&sample_timestamp_ns_, 0);
        last_callback_index_ = -1;

        stop_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!stop_event_) {
            sprintf_s(last_error_, sizeof(last_error_),
                      "B4C start FAILED: stop event Win32=%lu", GetLastError());
            return ASE_NoMemory;
        }

        if (!engine_.start_run()) {
            strcpy_s(last_error_, engine_.last_message());
            CloseHandle(stop_event_);
            stop_event_ = nullptr;
            return ASE_HWMalfunction;
        }

        InterlockedExchange(&worker_running_, 1);
        worker_thread_ = CreateThread(
            nullptr,
            0,
            &X4AsioDriver::worker_entry,
            this,
            0,
            &worker_thread_id_);
        if (!worker_thread_) {
            const DWORD error = GetLastError();
            InterlockedExchange(&worker_running_, 0);
            engine_.stop();
            CloseHandle(stop_event_);
            stop_event_ = nullptr;
            worker_thread_id_ = 0;
            sprintf_s(last_error_, sizeof(last_error_),
                      "B4C start FAILED: CreateThread Win32=%lu", error);
            return ASE_NoMemory;
        }

        sprintf_s(last_error_, sizeof(last_error_),
                  "B4C start OK workerThreadId=%lu timeInfo=%s position=0",
                  worker_thread_id_, time_info_mode_ ? "YES" : "NO");
        return ASE_OK;
    }

    ASIOError stop() override {
        bool worker_joined = true;

        if (worker_thread_) {
            if (stop_event_) SetEvent(stop_event_);
            const DWORD wait = WaitForSingleObject(worker_thread_, kWorkerJoinTimeoutMs);
            if (wait != WAIT_OBJECT_0) {
                worker_joined = false;
                sprintf_s(last_error_, sizeof(last_error_),
                          "B4C stop FAILED: worker join wait=%lu; hardware teardown WITHHELD",
                          wait);
                return ASE_HWMalfunction;
            }
            CloseHandle(worker_thread_);
            worker_thread_ = nullptr;
            worker_thread_id_ = 0;
        }

        if (stop_event_) {
            CloseHandle(stop_event_);
            stop_event_ = nullptr;
        }

        const bool state_ok = engine_.stop();
        const LONG worker_failed = InterlockedCompareExchange(&worker_failed_, 0, 0);
        const LONG callback_count = InterlockedCompareExchange(&callback_count_, 0, 0);
        const LONG index_errors = InterlockedCompareExchange(&callback_index_errors_, 0, 0);
        const LONG copy_errors = InterlockedCompareExchange(&dma_copy_errors_, 0, 0);
        const auto& stats = engine_.stats();

        if (!state_ok || worker_failed || index_errors || copy_errors ||
            stats.packet_discontinuities || stats.position_regressions) {
            sprintf_s(last_error_, sizeof(last_error_),
                      "B4C stop FAIL joined=%s workerFail=%ld cb=%ld idxErr=%ld copyErr=%ld pktErr=%lu posErr=%lu",
                      worker_joined ? "YES" : "NO", worker_failed, callback_count,
                      index_errors, copy_errors, stats.packet_discontinuities,
                      stats.position_regressions);
            return ASE_HWMalfunction;
        }

        sprintf_s(last_error_, sizeof(last_error_),
                  "B4C stop OK workerJoined=YES notif=%lu cb=%ld dmaWrites=%lu dmaFrames=%lu",
                  stats.notifications, callback_count,
                  stats.hardware_buffer_writes, stats.dma_frames_copied);
        return ASE_OK;
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
        *outputLatency = kHostBufferFrames;
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
        return sampleRate == kSampleRate ? ASE_OK : ASE_NoClock;
    }

    ASIOError getSampleRate(ASIOSampleRate* sampleRate) override {
        if (!sampleRate) return ASE_InvalidParameter;
        *sampleRate = kSampleRate;
        return ASE_OK;
    }

    ASIOError setSampleRate(ASIOSampleRate sampleRate) override {
        return sampleRate == kSampleRate ? ASE_OK : ASE_NoClock;
    }

    ASIOError getClockSources(ASIOClockSource* clocks, long* numSources) override {
        if (!numSources) return ASE_InvalidParameter;
        if (!clocks) {
            *numSources = 1;
            return ASE_OK;
        }
        if (*numSources < 1) {
            *numSources = 1;
            return ASE_InvalidParameter;
        }

        ZeroMemory(&clocks[0], sizeof(clocks[0]));
        clocks[0].index = 0;
        clocks[0].associatedChannel = -1;
        clocks[0].associatedGroup = -1;
        clocks[0].isCurrentSource = ASIOTrue;
        strcpy_s(clocks[0].name, "Internal");
        *numSources = 1;
        return ASE_OK;
    }

    ASIOError setClockSource(long reference) override {
        return reference == 0 ? ASE_OK : ASE_InvalidParameter;
    }

    ASIOError getSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp) override {
        if (!sPos || !tStamp) return ASE_InvalidParameter;
        if (!engine_.running() || !worker_thread_) return ASE_SPNotAdvancing;
        *sPos = static_cast<ASIOSamples>(InterlockedCompareExchange64(&sample_position_, 0, 0));
        *tStamp = static_cast<ASIOTimeStamp>(InterlockedCompareExchange64(&sample_timestamp_ns_, 0, 0));
        return ASE_OK;
    }

    ASIOError getChannelInfo(ASIOChannelInfo* info) override {
        if (!info) return ASE_InvalidParameter;
        if (info->isInput != ASIOFalse || info->channel < 0 || info->channel >= kHostChannels) {
            return ASE_InvalidParameter;
        }

        const long channel = info->channel;
        ZeroMemory(info, sizeof(*info));
        info->channel = channel;
        info->isInput = ASIOFalse;
        info->isActive = buffers_created_ ? ASIOTrue : ASIOFalse;
        info->channelGroup = 0;
        info->type = ASIOSTInt16LSB;
        strcpy_s(info->name, channel == 0 ? "X4 Output L" : "X4 Output R");
        return ASE_OK;
    }

    ASIOError createBuffers(
        ASIOBufferInfo* bufferInfos,
        long numChannels,
        long bufferSize,
        ASIOCallbacks* callbacks) override {

        if (!initialized_free_) {
            strcpy_s(last_error_, "B4C createBuffers rejected: init not FREE");
            return ASE_InvalidMode;
        }
        if (!bufferInfos || !callbacks || !callbacks->bufferSwitch) {
            strcpy_s(last_error_, "B4C createBuffers requires bufferInfos + legacy bufferSwitch fallback");
            return ASE_InvalidParameter;
        }
        if (numChannels != kHostChannels || bufferSize != kHostBufferFrames) {
            strcpy_s(last_error_, "B4C createBuffers requires 2 outputs x 512 frames");
            return ASE_InvalidParameter;
        }
        if (buffers_created_ || engine_.prepared()) {
            strcpy_s(last_error_, "B4C createBuffers rejected: already created");
            return ASE_InvalidMode;
        }

        bool seen_channel[2] = {false, false};
        for (long i = 0; i < numChannels; ++i) {
            const long channel = bufferInfos[i].channelNum;
            if (bufferInfos[i].isInput != ASIOFalse ||
                channel < 0 || channel >= kHostChannels || seen_channel[channel]) {
                strcpy_s(last_error_, "B4C requires unique output channels 0 and 1");
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

        time_info_mode_ = false;
        host_time_info_support_ = 0;
        if (callbacks_.asioMessage && callbacks_.bufferSwitchTimeInfo) {
            host_time_info_support_ =
                callbacks_.asioMessage(kAsioSupportsTimeInfo, 0, nullptr, nullptr);
            time_info_mode_ = host_time_info_support_ == 1;
        }

        for (long i = 0; i < numChannels; ++i) {
            const long channel = bufferInfos[i].channelNum;
            bufferInfos[i].buffers[0] = host_buffers_[channel][0];
            bufferInfos[i].buffers[1] = host_buffers_[channel][1];
        }

        buffers_created_ = true;
        sprintf_s(last_error_, sizeof(last_error_),
                  "B4C buffers ready timeInfo=%s; B4B query + B4A PCM preserved",
                  time_info_mode_ ? "YES" : "NO");
        return ASE_OK;
    }

    ASIOError disposeBuffers() override {
        ASIOError stop_result = ASE_OK;
        if (worker_thread_ || engine_.running()) {
            stop_result = stop();
            if (worker_thread_) {
                strcpy_s(last_error_, "B4C disposeBuffers WITHHELD: worker still not joined");
                return ASE_HWMalfunction;
            }
        }

        engine_.dispose();
        release_host_buffers();

        if (stop_result != ASE_OK) {
            strcpy_s(last_error_, "B4C disposeBuffers cleanup complete after worker failure");
            return stop_result;
        }

        strcpy_s(last_error_, "B4C disposeBuffers OK: time-info detached; buffers closed");
        return ASE_OK;
    }

    ASIOError controlPanel() override { return ASE_NotPresent; }

    ASIOError future(long selector, void* opt) override {
        (void)opt;
        if (selector == kAsioCanTimeInfo) return ASE_SUCCESS;
        return ASE_NotPresent;
    }

    ASIOError outputReady() override { return ASE_NotPresent; }

private:
    static DWORD WINAPI worker_entry(LPVOID context) {
        auto* self = static_cast<X4AsioDriver*>(context);
        if (!self) return ERROR_INVALID_PARAMETER;

        std::printf("B4C worker START thread=%lu timeInfo=%s\n",
                    GetCurrentThreadId(), self->time_info_mode_ ? "YES" : "NO");
        for (;;) {
            const X4WaveRtProcessResult result = self->engine_.process_one_notification(
                self->stop_event_, &X4AsioDriver::notification_observer, self);

            if (result == X4WaveRtProcessResult::StopRequested) {
                std::printf("B4C worker STOP requested thread=%lu\n", GetCurrentThreadId());
                break;
            }
            if (result == X4WaveRtProcessResult::Failed) {
                InterlockedExchange(&self->worker_failed_, 1);
                std::printf("B4C worker FAILED thread=%lu message=%s\n",
                            GetCurrentThreadId(), self->engine_.last_message());
                break;
            }
        }

        InterlockedExchange(&self->worker_running_, 0);
        std::printf("B4C worker EXIT thread=%lu\n", GetCurrentThreadId());
        return 0;
    }

    static bool notification_observer(
        void* context,
        ULONG zero_based_notification_index,
        ULONG packet_count) {

        auto* self = static_cast<X4AsioDriver*>(context);
        if (!self || !self->buffers_created_ || !self->callbacks_.bufferSwitch) return false;

        const ULONG write_packet = packet_count + 1;
        const long buffer_index = static_cast<long>(write_packet % kNotificationCount);

        if (self->last_callback_index_ >= 0 && self->last_callback_index_ == buffer_index) {
            InterlockedIncrement(&self->callback_index_errors_);
        }
        self->last_callback_index_ = buffer_index;
        const LONG callback_ordinal = InterlockedIncrement(&self->callback_count_);

        const ASIOSamples block_position =
            static_cast<ASIOSamples>(callback_ordinal - 1) * kHostBufferFrames;
        const ASIOTimeStamp block_timestamp = asio_system_time_ns();
        InterlockedExchange64(&self->sample_position_, block_position);
        InterlockedExchange64(&self->sample_timestamp_ns_, block_timestamp);

        if (self->time_info_mode_ && self->callbacks_.bufferSwitchTimeInfo) {
            ZeroMemory(&self->asio_time_, sizeof(self->asio_time_));
            self->asio_time_.timeInfo.speed = 1.0;
            self->asio_time_.timeInfo.systemTime = block_timestamp;
            self->asio_time_.timeInfo.samplePosition = block_position;
            self->asio_time_.timeInfo.sampleRate = kSampleRate;
            self->asio_time_.timeInfo.flags =
                kSystemTimeValid | kSamplePositionValid | kSampleRateValid | kSpeedValid;
            self->callbacks_.bufferSwitchTimeInfo(
                &self->asio_time_, buffer_index, ASIOFalse);
        } else {
            self->callbacks_.bufferSwitch(buffer_index, ASIOFalse);
        }

        const bool copied = self->engine_.write_interleaved_packet(
            write_packet,
            self->host_buffers_[0][buffer_index],
            self->host_buffers_[1][buffer_index],
            kHostBufferFrames);
        if (!copied) InterlockedIncrement(&self->dma_copy_errors_);

        std::printf(
            "B4C callback=%ld mode=%s notificationIndex=%lu packet=%lu writePacket=%lu slot=%ld samplePosition=%lld timestampNs=%lld copy=%s thread=%lu\n",
            callback_ordinal,
            self->time_info_mode_ ? "TIMEINFO" : "LEGACY",
            zero_based_notification_index,
            packet_count,
            write_packet,
            buffer_index,
            static_cast<long long>(block_position),
            static_cast<long long>(block_timestamp),
            copied ? "OK" : "FAIL",
            GetCurrentThreadId());
        return copied;
    }

    void force_join_worker() {
        if (worker_thread_) {
            if (stop_event_) SetEvent(stop_event_);
            WaitForSingleObject(worker_thread_, INFINITE);
            CloseHandle(worker_thread_);
            worker_thread_ = nullptr;
            worker_thread_id_ = 0;
        }
        if (stop_event_) {
            CloseHandle(stop_event_);
            stop_event_ = nullptr;
        }
        InterlockedExchange(&worker_running_, 0);
        if (engine_.running()) engine_.stop();
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
        asio_time_ = {};
        time_info_mode_ = false;
        host_time_info_support_ = 0;
        buffers_created_ = false;
        InterlockedExchange(&callback_count_, 0);
        InterlockedExchange(&callback_index_errors_, 0);
        InterlockedExchange(&dma_copy_errors_, 0);
        InterlockedExchange64(&sample_position_, 0);
        InterlockedExchange64(&sample_timestamp_ns_, 0);
        last_callback_index_ = -1;
        ZeroMemory(host_buffers_, sizeof(host_buffers_));
    }

    volatile LONG ref_count_ = 1;
    bool initialized_free_ = false;
    bool buffers_created_ = false;
    bool time_info_mode_ = false;
    long host_time_info_support_ = 0;
    X4WaveRtEngine engine_{};
    ASIOCallbacks callbacks_{};
    ASIOTime asio_time_{};
    ASIOBufferInfo* host_buffer_infos_ = nullptr;
    long host_buffer_info_count_ = 0;
    alignas(64) std::int16_t host_buffers_[2][2][kHostBufferFrames]{};
    HANDLE worker_thread_ = nullptr;
    HANDLE stop_event_ = nullptr;
    DWORD worker_thread_id_ = 0;
    volatile LONG worker_running_ = 0;
    volatile LONG worker_failed_ = 0;
    volatile LONG callback_count_ = 0;
    volatile LONG callback_index_errors_ = 0;
    volatile LONG dma_copy_errors_ = 0;
    volatile LONGLONG sample_position_ = 0;
    volatile LONGLONG sample_timestamp_ns_ = 0;
    long last_callback_index_ = -1;
    char last_error_[124] = "Stage B4C ASIO2 time-info not initialized";
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

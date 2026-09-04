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
#include "b5_identity.h"
#include "control_panel_b5.h"
#include "preflight.h"
#include "wavert_engine_b5.h"

#if !defined(_M_ARM64) || defined(_M_ARM64EC)
#error B5 driver source must be built for native Windows ARM64, not ARM64EC.
#endif

namespace {

constexpr long kInputChannels = 2;
constexpr long kOutputChannels = 2;
constexpr long kMinBufferFrames = 96;
constexpr long kMaxBufferFrames = 4800;
constexpr long kPreferredBufferFrames = 240;
constexpr long kMinBufferFrames192 = 384;
constexpr long kPreferredBufferFrames192 = 384;
constexpr long kBufferGranularity = 48;
constexpr long kB4DCompatibilityFrames = 512;
constexpr ULONG kNotificationCount = 2;
constexpr DWORD kWorkerJoinTimeoutMs = 2000;
constexpr ASIOSampleType kAsioInt24LSB = 17;
constexpr ULONG kBytesPerAsioSample = 3;
constexpr ULONG kRenderPinId = 1;
constexpr ULONG kCapturePinId = 4;

volatile LONG g_object_count = 0;
volatile LONG g_lock_count = 0;
HMODULE g_module = nullptr;

bool guid_equal(REFGUID a, REFGUID b) {
    return !!InlineIsEqualGUID(a, b);
}

ASIOTimeStamp asio_system_time_ns() {
    return static_cast<ASIOTimeStamp>(timeGetTime()) * 1000000LL;
}

bool supported_rate(ASIOSampleRate rate) {
    return rate == 48000.0 || rate == 96000.0 || rate == 192000.0;
}

bool capture_supported_rate(ASIOSampleRate rate) {
    return rate == 48000.0 || rate == 96000.0;
}

bool valid_buffer_size(long frames) {
    if (frames == kB4DCompatibilityFrames) return true;
    return frames >= kMinBufferFrames && frames <= kMaxBufferFrames &&
           (frames % kBufferGranularity) == 0;
}

class X4AsioDriverB5 final : public IASIO {
public:
    X4AsioDriverB5() { InterlockedIncrement(&g_object_count); }

    ~X4AsioDriverB5() {
        force_join_worker();
        release_host_buffers();
        render_.dispose();
        capture_.dispose();
        InterlockedDecrement(&g_object_count);
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (guid_equal(riid, IID_IUnknown) || guid_equal(riid, CLSID_X4_ARM64_ASIO_B5)) {
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
        owner_window_ = reinterpret_cast<HWND>(sysHandle);

        if (worker_thread_ || render_.running() || capture_.running()) {
            const ASIOError result = stop();
            if (result != ASE_OK && worker_thread_) {
                strcpy_s(last_error_, "B5 init rejected: existing worker could not be joined safely");
                return ASIOFalse;
            }
        }

        render_.dispose();
        capture_.dispose();
        release_host_buffers();
        initialized_free_ = false;
        sample_rate_ = 48000.0;
        InterlockedExchange64(&sample_position_, 0);
        InterlockedExchange64(&sample_timestamp_ns_, 0);

        // Immutable safety rule: the proven Render Pin 1 local + global gate
        // must be FREE before this driver will permit any later pin creation.
        const X4InstancePreflightResult preflight = run_x4_instance_preflight();
        if (!preflight.device_found) {
            strcpy_s(last_error_, "B5 init FAILED: X4 msft_wave not found");
            return ASIOFalse;
        }
        if (!preflight.filter_opened) {
            sprintf_s(last_error_, sizeof(last_error_),
                      "B5 init FAILED: filter open Win32=%lu", preflight.open_error);
            return ASIOFalse;
        }
        if (!preflight.local_ok || !preflight.global_ok ||
            preflight.local_possible == 0 || preflight.global_possible == 0) {
            sprintf_s(last_error_, sizeof(last_error_),
                      "B5 init INDETERMINATE: render C ok=%d %lu/%lu G ok=%d %lu/%lu; KsCreatePin SKIPPED",
                      preflight.local_ok ? 1 : 0,
                      preflight.local_current, preflight.local_possible,
                      preflight.global_ok ? 1 : 0,
                      preflight.global_current, preflight.global_possible);
            return ASIOFalse;
        }

        const bool busy =
            preflight.local_current >= preflight.local_possible ||
            preflight.global_current >= preflight.global_possible;
        if (busy) {
            sprintf_s(last_error_, sizeof(last_error_),
                      "B5 init BUSY: render C %lu/%lu G %lu/%lu; KsCreatePin SKIPPED",
                      preflight.local_current, preflight.local_possible,
                      preflight.global_current, preflight.global_possible);
            return ASIOFalse;
        }

        initialized_free_ = true;
        sprintf_s(last_error_, sizeof(last_error_),
                  "B5 init FREE: render C %lu/%lu G %lu/%lu; 24-bit 48/96/192 ready",
                  preflight.local_current, preflight.local_possible,
                  preflight.global_current, preflight.global_possible);
        return ASIOTrue;
    }

    void getDriverName(char* name) override {
        if (name) strcpy_s(name, 32, kX4AsioB5DriverName);
    }

    long getDriverVersion() override { return 200; }

    void getErrorMessage(char* string) override {
        if (string) strcpy_s(string, 124, last_error_);
    }

    ASIOError start() override {
        if (!initialized_free_ || !buffers_created_ || (!render_selected_ && !capture_selected_)) {
            strcpy_s(last_error_, "B5 start rejected: ASIO/WaveRT buffers not prepared");
            return ASE_InvalidMode;
        }
        if (worker_thread_ || render_.running() || capture_.running()) {
            strcpy_s(last_error_, "B5 start rejected: worker/RUN already active");
            return ASE_InvalidMode;
        }

        InterlockedExchange(&callback_count_, 0);
        InterlockedExchange(&callback_index_errors_, 0);
        InterlockedExchange(&dma_copy_errors_, 0);
        InterlockedExchange(&capture_copy_errors_, 0);
        InterlockedExchange(&worker_failed_, 0);
        InterlockedExchange64(&sample_position_, 0);
        InterlockedExchange64(&sample_timestamp_ns_, 0);
        last_callback_index_ = -1;

        stop_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!stop_event_) {
            sprintf_s(last_error_, sizeof(last_error_),
                      "B5 start FAILED: stop event Win32=%lu", GetLastError());
            return ASE_NoMemory;
        }

        // Capture starts first so a full-duplex callback can obtain its first
        // completed packet before the render-driven callback consumes it.
        if (capture_selected_ && !capture_.start_run()) {
            strcpy_s(last_error_, capture_.last_message());
            CloseHandle(stop_event_);
            stop_event_ = nullptr;
            return ASE_HWMalfunction;
        }
        if (render_selected_ && !render_.start_run()) {
            if (capture_.running()) capture_.stop();
            strcpy_s(last_error_, render_.last_message());
            CloseHandle(stop_event_);
            stop_event_ = nullptr;
            return ASE_HWMalfunction;
        }

        worker_thread_ = CreateThread(
            nullptr, 0, &X4AsioDriverB5::worker_entry,
            this, 0, &worker_thread_id_);
        if (!worker_thread_) {
            const DWORD error = GetLastError();
            if (render_.running()) render_.stop();
            if (capture_.running()) capture_.stop();
            CloseHandle(stop_event_);
            stop_event_ = nullptr;
            worker_thread_id_ = 0;
            sprintf_s(last_error_, sizeof(last_error_),
                      "B5 start FAILED: CreateThread Win32=%lu", error);
            return ASE_NoMemory;
        }

        sprintf_s(last_error_, sizeof(last_error_),
                  "B5 start OK rate=%.0f frames=%ld render=%d capture=%d thread=%lu timeInfo=%s",
                  sample_rate_, buffer_frames_, render_selected_ ? 1 : 0,
                  capture_selected_ ? 1 : 0, worker_thread_id_,
                  time_info_mode_ ? "YES" : "NO");
        return ASE_OK;
    }

    ASIOError stop() override {
        if (worker_thread_) {
            if (stop_event_) SetEvent(stop_event_);
            const DWORD wait = WaitForSingleObject(worker_thread_, kWorkerJoinTimeoutMs);
            if (wait != WAIT_OBJECT_0) {
                sprintf_s(last_error_, sizeof(last_error_),
                          "B5 stop FAILED: worker join wait=%lu; hardware teardown WITHHELD",
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

        bool state_ok = true;
        if (render_.running() && !render_.stop()) state_ok = false;
        if (capture_.running() && !capture_.stop()) state_ok = false;

        const LONG worker_failed = InterlockedCompareExchange(&worker_failed_, 0, 0);
        const LONG callbacks = InterlockedCompareExchange(&callback_count_, 0, 0);
        const LONG index_errors = InterlockedCompareExchange(&callback_index_errors_, 0, 0);
        const LONG render_copy_errors = InterlockedCompareExchange(&dma_copy_errors_, 0, 0);
        const LONG input_copy_errors = InterlockedCompareExchange(&capture_copy_errors_, 0, 0);
        const auto& rs = render_.stats();
        const auto& cs = capture_.stats();

        if (!state_ok || worker_failed || index_errors || render_copy_errors || input_copy_errors ||
            rs.packet_discontinuities || rs.position_regressions || cs.packet_discontinuities) {
            sprintf_s(last_error_, sizeof(last_error_),
                      "B5 stop FAIL cb=%ld worker=%ld idx=%ld outCopy=%ld inCopy=%ld rPkt=%lu rPos=%lu cPkt=%lu",
                      callbacks, worker_failed, index_errors,
                      render_copy_errors, input_copy_errors,
                      rs.packet_discontinuities, rs.position_regressions,
                      cs.packet_discontinuities);
            return ASE_HWMalfunction;
        }

        sprintf_s(last_error_, sizeof(last_error_),
                  "B5 stop OK workerJoined=YES cb=%ld renderNotif=%lu captureNotif=%lu outFrames=%lu inFrames=%lu",
                  callbacks, rs.notifications, cs.notifications,
                  rs.dma_frames_copied, cs.dma_frames_copied);
        return ASE_OK;
    }

    ASIOError getChannels(long* numInputChannels, long* numOutputChannels) override {
        if (!numInputChannels || !numOutputChannels) return ASE_InvalidParameter;
        *numInputChannels = capture_supported_rate(sample_rate_) ? kInputChannels : 0;
        *numOutputChannels = kOutputChannels;
        return ASE_OK;
    }

    ASIOError getLatencies(long* inputLatency, long* outputLatency) override {
        if (!inputLatency || !outputLatency) return ASE_InvalidParameter;
        const long preferred = sample_rate_ == 192000.0 ?
            kPreferredBufferFrames192 : kPreferredBufferFrames;
        const long frames = buffers_created_ ? buffer_frames_ : preferred;
        *inputLatency = capture_supported_rate(sample_rate_) ? frames : 0;
        *outputLatency = frames;
        return ASE_OK;
    }

    ASIOError getBufferSize(long* minSize, long* maxSize, long* preferredSize, long* granularity) override {
        if (!minSize || !maxSize || !preferredSize || !granularity) return ASE_InvalidParameter;
        const bool rate_192 = sample_rate_ == 192000.0;
        *minSize = rate_192 ? kMinBufferFrames192 : kMinBufferFrames;
        *maxSize = kMaxBufferFrames;
        *preferredSize = b5_load_preferred_buffer_frames(sample_rate_);
        *granularity = kBufferGranularity;
        return ASE_OK;
    }

    ASIOError canSampleRate(ASIOSampleRate sampleRate) override {
        return supported_rate(sampleRate) ? ASE_OK : ASE_NoClock;
    }

    ASIOError getSampleRate(ASIOSampleRate* sampleRate) override {
        if (!sampleRate) return ASE_InvalidParameter;
        *sampleRate = sample_rate_;
        return ASE_OK;
    }

    ASIOError setSampleRate(ASIOSampleRate sampleRate) override {
        if (!supported_rate(sampleRate)) return ASE_NoClock;
        if (buffers_created_ || worker_thread_ || render_.running() || capture_.running()) {
            strcpy_s(last_error_, "B5 setSampleRate rejected while buffers/RUN are active");
            return ASE_InvalidMode;
        }
        sample_rate_ = sampleRate;
        sprintf_s(last_error_, sizeof(last_error_), "B5 sample rate selected %.0f Hz", sample_rate_);
        return ASE_OK;
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
        strcpy_s(clocks[0].name, "Internal Clock");
        *numSources = 1;
        return ASE_OK;
    }

    ASIOError setClockSource(long reference) override {
        return reference == 0 ? ASE_OK : ASE_InvalidParameter;
    }

    ASIOError getSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp) override {
        if (!sPos || !tStamp) return ASE_InvalidParameter;
        if (!worker_thread_ || (!render_.running() && !capture_.running())) return ASE_SPNotAdvancing;
        *sPos = static_cast<ASIOSamples>(InterlockedCompareExchange64(&sample_position_, 0, 0));
        *tStamp = static_cast<ASIOTimeStamp>(InterlockedCompareExchange64(&sample_timestamp_ns_, 0, 0));
        return ASE_OK;
    }

    ASIOError getChannelInfo(ASIOChannelInfo* info) override {
        if (!info) return ASE_InvalidParameter;
        const bool input = info->isInput != ASIOFalse;
        const long channel = info->channel;
        if (channel < 0 || channel >= 2) return ASE_InvalidParameter;
        if (input && !capture_supported_rate(sample_rate_)) return ASE_InvalidParameter;

        ZeroMemory(info, sizeof(*info));
        info->channel = channel;
        info->isInput = input ? ASIOTrue : ASIOFalse;
        info->isActive = input ?
            (input_selected_[channel] ? ASIOTrue : ASIOFalse) :
            (output_selected_[channel] ? ASIOTrue : ASIOFalse);
        info->channelGroup = 0;
        info->type = kAsioInt24LSB;
        if (input) {
            strcpy_s(info->name, channel == 0 ? "X4 Audio-In L" : "X4 Audio-In R");
        } else {
            strcpy_s(info->name, channel == 0 ? "X4 Front L" : "X4 Front R");
        }
        return ASE_OK;
    }

    ASIOError createBuffers(
        ASIOBufferInfo* bufferInfos,
        long numChannels,
        long bufferSize,
        ASIOCallbacks* callbacks) override {

        if (!initialized_free_) {
            strcpy_s(last_error_, "B5 createBuffers rejected: init not FREE");
            return ASE_InvalidMode;
        }
        if (!bufferInfos || numChannels <= 0 || !callbacks || !callbacks->bufferSwitch) {
            strcpy_s(last_error_, "B5 createBuffers requires channels + legacy bufferSwitch fallback");
            return ASE_InvalidParameter;
        }
        const long selected_min = sample_rate_ == 192000.0 ?
            kMinBufferFrames192 : kMinBufferFrames;
        if (!valid_buffer_size(bufferSize) || bufferSize < selected_min) {
            sprintf_s(last_error_, sizeof(last_error_),
                      "B5 createBuffers invalid frames=%ld rate=%.0f; expected %ld..4800 step48 (512 compatibility accepted)",
                      bufferSize, sample_rate_, selected_min);
            return ASE_InvalidParameter;
        }
        if (buffers_created_ || render_.prepared() || capture_.prepared()) {
            strcpy_s(last_error_, "B5 createBuffers rejected: already created");
            return ASE_InvalidMode;
        }

        bool seen_input[2] = {false, false};
        bool seen_output[2] = {false, false};
        bool any_input = false;
        bool any_output = false;
        for (long i = 0; i < numChannels; ++i) {
            const long channel = bufferInfos[i].channelNum;
            if (channel < 0 || channel >= 2) {
                strcpy_s(last_error_, "B5 createBuffers channel index must be 0 or 1");
                return ASE_InvalidParameter;
            }
            if (bufferInfos[i].isInput != ASIOFalse) {
                if (!capture_supported_rate(sample_rate_) || seen_input[channel]) {
                    strcpy_s(last_error_, "B5 createBuffers input unsupported/duplicate at selected sample rate");
                    return ASE_InvalidParameter;
                }
                seen_input[channel] = true;
                any_input = true;
            } else {
                if (seen_output[channel]) {
                    strcpy_s(last_error_, "B5 createBuffers duplicate output channel");
                    return ASE_InvalidParameter;
                }
                seen_output[channel] = true;
                any_output = true;
            }
        }
        if (!any_input && !any_output) return ASE_InvalidParameter;

        const X4WaveRtB5Config render_config{
            X4WaveRtB5Direction::Render,
            kRenderPinId,
            static_cast<ULONG>(sample_rate_),
            2, 24,
            static_cast<ULONG>(bufferSize),
            kNotificationCount};
        const X4WaveRtB5Config capture_config{
            X4WaveRtB5Direction::Capture,
            kCapturePinId,
            static_cast<ULONG>(sample_rate_),
            2, 24,
            static_cast<ULONG>(bufferSize),
            kNotificationCount};

        if (any_output) {
            const X4WaveRtB5PrepareResult result = render_.prepare(render_config);
            if (result != X4WaveRtB5PrepareResult::Ready) {
                strcpy_s(last_error_, render_.last_message());
                render_.dispose();
                return ASE_HWMalfunction;
            }
        }
        if (any_input) {
            const X4WaveRtB5PrepareResult result = capture_.prepare(capture_config);
            if (result != X4WaveRtB5PrepareResult::Ready) {
                strcpy_s(last_error_, capture_.last_message());
                capture_.dispose();
                render_.dispose();
                return ASE_HWMalfunction;
            }
        }

        ZeroMemory(output_buffers_, sizeof(output_buffers_));
        ZeroMemory(input_buffers_, sizeof(input_buffers_));
        ZeroMemory(zero_buffer_, sizeof(zero_buffer_));
        output_selected_[0] = seen_output[0];
        output_selected_[1] = seen_output[1];
        input_selected_[0] = seen_input[0];
        input_selected_[1] = seen_input[1];
        render_selected_ = any_output;
        capture_selected_ = any_input;
        buffer_frames_ = bufferSize;
        callbacks_ = *callbacks;
        host_buffer_infos_ = bufferInfos;
        host_buffer_info_count_ = numChannels;

        time_info_mode_ = false;
        host_time_info_support_ = 0;
        if (callbacks_.asioMessage && callbacks_.bufferSwitchTimeInfo) {
            host_time_info_support_ = callbacks_.asioMessage(kAsioSupportsTimeInfo, 0, nullptr, nullptr);
            time_info_mode_ = host_time_info_support_ == 1;
        }

        for (long i = 0; i < numChannels; ++i) {
            const long channel = bufferInfos[i].channelNum;
            if (bufferInfos[i].isInput != ASIOFalse) {
                bufferInfos[i].buffers[0] = input_buffers_[channel][0];
                bufferInfos[i].buffers[1] = input_buffers_[channel][1];
            } else {
                bufferInfos[i].buffers[0] = output_buffers_[channel][0];
                bufferInfos[i].buffers[1] = output_buffers_[channel][1];
            }
        }

        buffers_created_ = true;
        sprintf_s(last_error_, sizeof(last_error_),
                  "B5 buffers ready rate=%.0f frames=%ld Int24LSB render=%d capture=%d timeInfo=%s",
                  sample_rate_, buffer_frames_, render_selected_ ? 1 : 0,
                  capture_selected_ ? 1 : 0, time_info_mode_ ? "YES" : "NO");
        return ASE_OK;
    }

    ASIOError disposeBuffers() override {
        ASIOError stop_result = ASE_OK;
        if (worker_thread_ || render_.running() || capture_.running()) {
            stop_result = stop();
            if (worker_thread_) {
                strcpy_s(last_error_, "B5 disposeBuffers WITHHELD: worker still not joined");
                return ASE_HWMalfunction;
            }
        }

        render_.dispose();
        capture_.dispose();
        release_host_buffers();
        if (stop_result != ASE_OK) return stop_result;
        strcpy_s(last_error_, "B5 disposeBuffers OK");
        return ASE_OK;
    }

    ASIOError controlPanel() override {
        if (InterlockedCompareExchange(&control_panel_open_, 1, 0) != 0) {
            return ASE_InvalidMode;
        }

        B5ControlPanelState state{};
        state.sample_rate = sample_rate_;
        state.active_buffer_frames = buffers_created_ ? buffer_frames_ : 0;
        state.buffers_created = buffers_created_;
        state.worker_running = worker_thread_ != nullptr;
        strcpy_s(state.last_status, last_error_);

        HWND owner = owner_window_ && IsWindow(owner_window_) ? owner_window_ : nullptr;
        const bool shown = b5_show_control_panel(g_module, owner, state);
        InterlockedExchange(&control_panel_open_, 0);

        if (!shown) {
            strcpy_s(last_error_, "B5 control panel could not be created");
            return ASE_HWMalfunction;
        }
        return ASE_OK;
    }

    ASIOError future(long selector, void* opt) override {
        (void)opt;
        if (selector == kAsioCanTimeInfo) return ASE_SUCCESS;
        return ASE_NotPresent;
    }

    ASIOError outputReady() override { return ASE_NotPresent; }

private:
    static DWORD WINAPI worker_entry(LPVOID context) {
        auto* self = static_cast<X4AsioDriverB5*>(context);
        if (!self) return ERROR_INVALID_PARAMETER;
        self->worker_loop();
        return 0;
    }

    void worker_loop() {
        std::printf("B5 worker START thread=%lu rate=%.0f frames=%ld render=%d capture=%d\n",
                    GetCurrentThreadId(), sample_rate_, buffer_frames_,
                    render_selected_ ? 1 : 0, capture_selected_ ? 1 : 0);

        for (;;) {
            ULONG master_packet = 0;
            long buffer_index = 0;

            if (render_selected_) {
                const X4WaveRtB5ProcessResult result =
                    render_.process_one_notification(stop_event_, 250, nullptr, nullptr);
                if (result == X4WaveRtB5ProcessResult::StopRequested) break;
                if (result == X4WaveRtB5ProcessResult::Failed) {
                    InterlockedExchange(&worker_failed_, 1);
                    std::printf("B5 worker RENDER failed: %s\n", render_.last_message());
                    break;
                }
                master_packet = render_.stats().last_packet;
                const ULONG write_packet = master_packet + 1;
                buffer_index = static_cast<long>(write_packet % kNotificationCount);

                if (capture_selected_) {
                    const X4WaveRtB5ProcessResult capture_result =
                        capture_.process_one_notification(stop_event_, 250, nullptr, nullptr);
                    if (capture_result == X4WaveRtB5ProcessResult::StopRequested) break;
                    if (capture_result == X4WaveRtB5ProcessResult::Failed) {
                        InterlockedExchange(&worker_failed_, 1);
                        std::printf("B5 worker CAPTURE failed: %s\n", capture_.last_message());
                        break;
                    }
                    const ULONG capture_packet = capture_.stats().last_packet;
                    if (!capture_.read_capture_packet24(
                            capture_packet,
                            input_buffers_[0][buffer_index],
                            input_buffers_[1][buffer_index],
                            static_cast<ULONG>(buffer_frames_))) {
                        InterlockedIncrement(&capture_copy_errors_);
                    }
                }
            } else {
                const X4WaveRtB5ProcessResult result =
                    capture_.process_one_notification(stop_event_, 250, nullptr, nullptr);
                if (result == X4WaveRtB5ProcessResult::StopRequested) break;
                if (result == X4WaveRtB5ProcessResult::Failed) {
                    InterlockedExchange(&worker_failed_, 1);
                    std::printf("B5 worker CAPTURE failed: %s\n", capture_.last_message());
                    break;
                }
                master_packet = capture_.stats().last_packet;
                buffer_index = static_cast<long>(master_packet % kNotificationCount);
                if (!capture_.read_capture_packet24(
                        master_packet,
                        input_buffers_[0][buffer_index],
                        input_buffers_[1][buffer_index],
                        static_cast<ULONG>(buffer_frames_))) {
                    InterlockedIncrement(&capture_copy_errors_);
                }
            }

            if (last_callback_index_ >= 0 && last_callback_index_ == buffer_index) {
                InterlockedIncrement(&callback_index_errors_);
            }
            last_callback_index_ = buffer_index;
            const LONG callback_ordinal = InterlockedIncrement(&callback_count_);
            const ASIOSamples block_position =
                static_cast<ASIOSamples>(callback_ordinal - 1) * buffer_frames_;
            const ASIOTimeStamp block_timestamp = asio_system_time_ns();
            InterlockedExchange64(&sample_position_, block_position);
            InterlockedExchange64(&sample_timestamp_ns_, block_timestamp);

            if (time_info_mode_ && callbacks_.bufferSwitchTimeInfo) {
                ZeroMemory(&asio_time_, sizeof(asio_time_));
                asio_time_.timeInfo.speed = 1.0;
                asio_time_.timeInfo.systemTime = block_timestamp;
                asio_time_.timeInfo.samplePosition = block_position;
                asio_time_.timeInfo.sampleRate = sample_rate_;
                asio_time_.timeInfo.flags =
                    kSystemTimeValid | kSamplePositionValid | kSampleRateValid | kSpeedValid;
                callbacks_.bufferSwitchTimeInfo(&asio_time_, buffer_index, ASIOFalse);
            } else {
                callbacks_.bufferSwitch(buffer_index, ASIOFalse);
            }

            if (render_selected_) {
                const ULONG write_packet = master_packet + 1;
                const std::uint8_t* left = output_selected_[0] ?
                    output_buffers_[0][buffer_index] : zero_buffer_;
                const std::uint8_t* right = output_selected_[1] ?
                    output_buffers_[1][buffer_index] : zero_buffer_;
                if (!render_.write_render_packet24(
                        write_packet, left, right,
                        static_cast<ULONG>(buffer_frames_))) {
                    InterlockedIncrement(&dma_copy_errors_);
                }
            }
        }

        std::printf("B5 worker EXIT thread=%lu\n", GetCurrentThreadId());
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
        if (render_.running()) render_.stop();
        if (capture_.running()) capture_.stop();
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
        render_selected_ = false;
        capture_selected_ = false;
        output_selected_[0] = output_selected_[1] = false;
        input_selected_[0] = input_selected_[1] = false;
        buffer_frames_ = kPreferredBufferFrames;
        InterlockedExchange(&callback_count_, 0);
        InterlockedExchange(&callback_index_errors_, 0);
        InterlockedExchange(&dma_copy_errors_, 0);
        InterlockedExchange(&capture_copy_errors_, 0);
        InterlockedExchange64(&sample_position_, 0);
        InterlockedExchange64(&sample_timestamp_ns_, 0);
        last_callback_index_ = -1;
        ZeroMemory(output_buffers_, sizeof(output_buffers_));
        ZeroMemory(input_buffers_, sizeof(input_buffers_));
        ZeroMemory(zero_buffer_, sizeof(zero_buffer_));
    }

    volatile LONG ref_count_ = 1;
    bool initialized_free_ = false;
    bool buffers_created_ = false;
    bool render_selected_ = false;
    bool capture_selected_ = false;
    bool output_selected_[2] = {false, false};
    bool input_selected_[2] = {false, false};
    bool time_info_mode_ = false;
    long host_time_info_support_ = 0;
    ASIOSampleRate sample_rate_ = 48000.0;
    long buffer_frames_ = kPreferredBufferFrames;
    HWND owner_window_ = nullptr;
    volatile LONG control_panel_open_ = 0;

    X4WaveRtEngineB5 render_{};
    X4WaveRtEngineB5 capture_{};
    ASIOCallbacks callbacks_{};
    ASIOTime asio_time_{};
    ASIOBufferInfo* host_buffer_infos_ = nullptr;
    long host_buffer_info_count_ = 0;

    alignas(64) std::uint8_t output_buffers_[2][2][kMaxBufferFrames * kBytesPerAsioSample]{};
    alignas(64) std::uint8_t input_buffers_[2][2][kMaxBufferFrames * kBytesPerAsioSample]{};
    alignas(64) std::uint8_t zero_buffer_[kMaxBufferFrames * kBytesPerAsioSample]{};

    HANDLE worker_thread_ = nullptr;
    HANDLE stop_event_ = nullptr;
    DWORD worker_thread_id_ = 0;
    volatile LONG worker_failed_ = 0;
    volatile LONG callback_count_ = 0;
    volatile LONG callback_index_errors_ = 0;
    volatile LONG dma_copy_errors_ = 0;
    volatile LONG capture_copy_errors_ = 0;
    volatile LONGLONG sample_position_ = 0;
    volatile LONGLONG sample_timestamp_ns_ = 0;
    long last_callback_index_ = -1;
    char last_error_[124] = "B5 ASIO not initialized";
};

class X4ClassFactoryB5 final : public IClassFactory {
public:
    X4ClassFactoryB5() { InterlockedIncrement(&g_object_count); }
    ~X4ClassFactoryB5() { InterlockedDecrement(&g_object_count); }

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
        auto* object = new (std::nothrow) X4AsioDriverB5();
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
    swprintf_s(clsid_key, L"SOFTWARE\\Classes\\CLSID\\%ls", kX4AsioB5ClsidString);
    swprintf_s(inproc_key, L"%ls\\InprocServer32", clsid_key);
    swprintf_s(asio_key, L"SOFTWARE\\ASIO\\%ls", kX4AsioB5RegistryName);

    HRESULT hr = set_registry_string(HKEY_LOCAL_MACHINE, clsid_key, nullptr, kX4AsioB5Description);
    if (FAILED(hr)) return hr;
    hr = set_registry_string(HKEY_LOCAL_MACHINE, inproc_key, nullptr, module_path);
    if (FAILED(hr)) return hr;
    hr = set_registry_string(HKEY_LOCAL_MACHINE, inproc_key, L"ThreadingModel", L"Apartment");
    if (FAILED(hr)) return hr;
    hr = set_registry_string(HKEY_LOCAL_MACHINE, asio_key, L"CLSID", kX4AsioB5ClsidString);
    if (FAILED(hr)) return hr;
    return set_registry_string(HKEY_LOCAL_MACHINE, asio_key, L"Description", kX4AsioB5Description);
}

HRESULT unregister_server() {
    wchar_t clsid_key[160]{};
    wchar_t asio_key[256]{};
    swprintf_s(clsid_key, L"SOFTWARE\\Classes\\CLSID\\%ls", kX4AsioB5ClsidString);
    swprintf_s(asio_key, L"SOFTWARE\\ASIO\\%ls", kX4AsioB5RegistryName);

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
    if (!guid_equal(rclsid, CLSID_X4_ARM64_ASIO_B5)) return CLASS_E_CLASSNOTAVAILABLE;
    auto* factory = new (std::nothrow) X4ClassFactoryB5();
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

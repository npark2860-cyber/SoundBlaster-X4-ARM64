// B5 high-rate worker adapter.
//
// Included by the ARM64EC and Classic ARM64 driver translation units after
// driver_b5.cpp. The driver class is translation-unit-local and exposed only
// to this adapter; WaveRT engine internals remain private and are reached only
// through the public signaled-notification API.

namespace {

constexpr char kB5MuxAdapter[] = "dual-event-mux-v3";
constexpr char kB5RuntimeFailsafe[] = "runtime-failsafe-v1";
constexpr wchar_t kB5RuntimeFailureFileName[] = L"B5_RUNTIME_FAILURE.txt";
constexpr ULONG kB5CaptureStarvationLimit = 4;

bool b5_emergency_silence_render(X4AsioDriverB5* self) {
    if (!self || !self->render_selected_) return true;
    if (!self->render_.prepared()) return false;

    // Do not stop/dispose the pin here. The worker must return first so the
    // existing joined-worker-before-hardware-teardown rule remains intact.
    // Both WaveRT notification slots are overwritten with silence so a worker
    // failure cannot leave the last audible cyclic contents repeating forever.
    const ULONG frames = static_cast<ULONG>(self->buffer_frames_);
    const bool slot0 = self->render_.write_render_packet24(
        0, self->zero_buffer_, self->zero_buffer_, frames);
    const bool slot1 = self->render_.write_render_packet24(
        1, self->zero_buffer_, self->zero_buffer_, frames);
    return slot0 && slot1;
}

void b5_write_runtime_failure_record(
    X4AsioDriverB5* self,
    const char* direction,
    const char* failure_message,
    const char* render_message,
    const char* capture_message,
    DWORD worker_win32_error,
    bool silence_ok,
    const X4WaveRtB5Stats& render_stats,
    const X4WaveRtB5Stats& capture_stats,
    ULONG capture_not_ready,
    ULONG capture_more_data,
    ULONG capture_phase_misses,
    ULONG capture_packets_consumed) {

    if (!self) return;

    SYSTEMTIME now{};
    GetLocalTime(&now);

    const LONG callbacks = InterlockedCompareExchange(&self->callback_count_, 0, 0);
    const LONG index_errors = InterlockedCompareExchange(&self->callback_index_errors_, 0, 0);
    const LONG render_copy_errors = InterlockedCompareExchange(&self->dma_copy_errors_, 0, 0);
    const LONG capture_copy_errors = InterlockedCompareExchange(&self->capture_copy_errors_, 0, 0);

    char record[4096]{};
    sprintf_s(
        record, sizeof(record),
        "Sound Blaster X4 ARM64 ASIO B5 runtime failure\r\n"
        "marker=%s adapter=%s\r\n"
        "timeLocal=%04u-%02u-%02u %02u:%02u:%02u.%03u tickMs=%llu\r\n"
        "processId=%lu threadId=%lu\r\n"
        "direction=%s reason=%s workerWin32=%lu emergencySilence=%s\r\n"
        "rate=%.0f frames=%ld renderSelected=%d captureSelected=%d renderRunning=%d captureRunning=%d\r\n"
        "callbacks=%ld lastCallbackIndex=%ld indexErrors=%ld renderCopyErrors=%ld captureCopyErrors=%ld\r\n"
        "render notifications=%lu packetDiscontinuities=%lu positionRegressions=%lu writes=%lu framesCopied=%lu nonzeroSamples=%lu lastPacket=%lu\r\n"
        "capture notifications=%lu packetDiscontinuities=%lu positionRegressions=%lu writes=%lu framesCopied=%lu nonzeroSamples=%lu lastPacket=%lu\r\n"
        "captureNotReady=%lu captureMoreData=%lu capturePhaseMisses=%lu captureConsumed=%lu\r\n"
        "renderMessage=%s\r\n"
        "captureMessage=%s\r\n"
        "logPath=%%TEMP%%\\B5_RUNTIME_FAILURE.txt\r\n",
        kB5RuntimeFailsafe, kB5MuxAdapter,
        now.wYear, now.wMonth, now.wDay,
        now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
        static_cast<unsigned long long>(GetTickCount64()),
        GetCurrentProcessId(), GetCurrentThreadId(),
        direction ? direction : "unknown",
        failure_message ? failure_message : "unknown",
        worker_win32_error,
        silence_ok ? "OK" : "FAILED",
        self->sample_rate_, self->buffer_frames_,
        self->render_selected_ ? 1 : 0,
        self->capture_selected_ ? 1 : 0,
        self->render_.running() ? 1 : 0,
        self->capture_.running() ? 1 : 0,
        callbacks, self->last_callback_index_, index_errors,
        render_copy_errors, capture_copy_errors,
        render_stats.notifications,
        render_stats.packet_discontinuities,
        render_stats.position_regressions,
        render_stats.hardware_buffer_writes,
        render_stats.dma_frames_copied,
        render_stats.dma_nonzero_samples,
        render_stats.last_packet,
        capture_stats.notifications,
        capture_stats.packet_discontinuities,
        capture_stats.position_regressions,
        capture_stats.hardware_buffer_writes,
        capture_stats.dma_frames_copied,
        capture_stats.dma_nonzero_samples,
        capture_stats.last_packet,
        capture_not_ready, capture_more_data,
        capture_phase_misses, capture_packets_consumed,
        render_message ? render_message : "",
        capture_message ? capture_message : "");

    // Always expose the record to DebugView/attached debuggers even if file I/O
    // is unavailable. File output is failure-only and occurs after silence.
    OutputDebugStringA(record);

    wchar_t log_path[MAX_PATH]{};
    wchar_t temp_path[MAX_PATH]{};
    constexpr DWORD temp_chars = static_cast<DWORD>(sizeof(temp_path) / sizeof(temp_path[0]));
    const DWORD temp_len = GetTempPathW(temp_chars, temp_path);
    if (temp_len > 0 && temp_len < temp_chars) {
        swprintf_s(
            log_path, sizeof(log_path) / sizeof(log_path[0]),
            L"%ls%ls", temp_path, kB5RuntimeFailureFileName);
    } else {
        wcscpy_s(
            log_path, sizeof(log_path) / sizeof(log_path[0]),
            kB5RuntimeFailureFileName);
    }

    HANDLE file = CreateFileW(
        log_path,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        char file_error[160]{};
        sprintf_s(
            file_error, sizeof(file_error),
            "B5 runtime failure log CreateFileW FAILED Win32=%lu\n",
            GetLastError());
        OutputDebugStringA(file_error);
        std::printf("%s", file_error);
        return;
    }

    DWORD written = 0;
    const DWORD bytes = static_cast<DWORD>(std::strlen(record));
    const BOOL write_ok = WriteFile(file, record, bytes, &written, nullptr);
    FlushFileBuffers(file);
    CloseHandle(file);

    if (!write_ok || written != bytes) {
        char write_error[192]{};
        sprintf_s(
            write_error, sizeof(write_error),
            "B5 runtime failure log WriteFile incomplete ok=%d written=%lu expected=%lu Win32=%lu\n",
            write_ok ? 1 : 0, written, bytes, GetLastError());
        OutputDebugStringA(write_error);
        std::printf("%s", write_error);
    } else {
        OutputDebugStringW(L"B5 runtime failure log written: ");
        OutputDebugStringW(log_path);
        OutputDebugStringW(L"\n");
    }
}

bool b5_mux_dispatch_callback(
    X4AsioDriverB5* self,
    ULONG master_packet,
    long buffer_index) {

    if (self->last_callback_index_ >= 0 &&
        self->last_callback_index_ == buffer_index) {
        InterlockedIncrement(&self->callback_index_errors_);
        return false;
    }
    self->last_callback_index_ = buffer_index;

    const LONG callback_ordinal = InterlockedIncrement(&self->callback_count_);
    const ASIOSamples block_position =
        static_cast<ASIOSamples>(callback_ordinal - 1) * self->buffer_frames_;
    const ASIOTimeStamp block_timestamp = asio_system_time_ns();
    InterlockedExchange64(&self->sample_position_, block_position);
    InterlockedExchange64(&self->sample_timestamp_ns_, block_timestamp);

    if (self->time_info_mode_ && self->callbacks_.bufferSwitchTimeInfo) {
        ZeroMemory(&self->asio_time_, sizeof(self->asio_time_));
        self->asio_time_.timeInfo.speed = 1.0;
        self->asio_time_.timeInfo.systemTime = block_timestamp;
        self->asio_time_.timeInfo.samplePosition = block_position;
        self->asio_time_.timeInfo.sampleRate = self->sample_rate_;
        self->asio_time_.timeInfo.flags =
            kSystemTimeValid | kSamplePositionValid | kSampleRateValid | kSpeedValid;
        self->callbacks_.bufferSwitchTimeInfo(
            &self->asio_time_, buffer_index, ASIOFalse);
    } else {
        self->callbacks_.bufferSwitch(buffer_index, ASIOFalse);
    }

    if (self->render_selected_) {
        const ULONG write_packet = master_packet + 1;
        const std::uint8_t* left = self->output_selected_[0] ?
            self->output_buffers_[0][buffer_index] : self->zero_buffer_;
        const std::uint8_t* right = self->output_selected_[1] ?
            self->output_buffers_[1][buffer_index] : self->zero_buffer_;
        if (!self->render_.write_render_packet24(
                write_packet, left, right,
                static_cast<ULONG>(self->buffer_frames_))) {
            InterlockedIncrement(&self->dma_copy_errors_);
            return false;
        }
    }

    return true;
}

bool b5_mux_render_event(X4AsioDriverB5* self, ULONG* packet_out) {
    const auto before = self->render_.stats();
    ULONG packet = 0;
    BOOL more_data = FALSE;
    const X4WaveRtB5ProcessResult result =
        self->render_.process_signaled_notification(
            &packet, &more_data, nullptr, nullptr, false);

    if (result != X4WaveRtB5ProcessResult::Notification) return false;

    const auto after = self->render_.stats();
    if (after.packet_discontinuities != before.packet_discontinuities ||
        after.position_regressions != before.position_regressions) {
        return false;
    }

    *packet_out = packet;
    return true;
}

X4WaveRtB5ProcessResult b5_mux_capture_event(
    X4AsioDriverB5* self,
    ULONG* packet_out,
    BOOL* more_data_out) {

    const ULONG before_discontinuities =
        self->capture_.stats().packet_discontinuities;
    const X4WaveRtB5ProcessResult result =
        self->capture_.process_signaled_notification(
            packet_out, more_data_out, nullptr, nullptr, false);

    if (result == X4WaveRtB5ProcessResult::Notification &&
        self->capture_.stats().packet_discontinuities != before_discontinuities) {
        return X4WaveRtB5ProcessResult::Failed;
    }
    return result;
}

DWORD b5_mux_worker_loop(X4AsioDriverB5* self) {
    if (!self || !self->stop_event_) return ERROR_INVALID_PARAMETER;

    std::printf(
        "B5 worker START adapter=%s failsafe=%s thread=%lu rate=%.0f frames=%ld render=%d capture=%d\n",
        kB5MuxAdapter, kB5RuntimeFailsafe,
        GetCurrentThreadId(), self->sample_rate_, self->buffer_frames_,
        self->render_selected_ ? 1 : 0, self->capture_selected_ ? 1 : 0);

    ULONG capture_not_ready = 0;
    ULONG capture_more_data = 0;
    ULONG capture_phase_misses = 0;
    ULONG capture_packets_consumed = 0;

    auto fail_worker = [&](const char* direction, const char* message) -> DWORD {
        // Capture the diagnostic state before the emergency silence writes can
        // alter render write counters or the engine's last-message buffer.
        const DWORD worker_win32_error = GetLastError();
        const X4WaveRtB5Stats render_stats = self->render_.stats();
        const X4WaveRtB5Stats capture_stats = self->capture_.stats();
        char failure_message[224]{};
        char render_message[224]{};
        char capture_message[224]{};
        strcpy_s(failure_message, message ? message : "unknown");
        strcpy_s(render_message, self->render_.last_message());
        strcpy_s(capture_message, self->capture_.last_message());

        InterlockedExchange(&self->worker_failed_, 1);

        // Silence first. File/debug logging is intentionally after this call so
        // a blocked filesystem cannot prolong a repeating last-buffer tone.
        const bool silence_ok = b5_emergency_silence_render(self);

        b5_write_runtime_failure_record(
            self,
            direction,
            failure_message,
            render_message,
            capture_message,
            worker_win32_error,
            silence_ok,
            render_stats,
            capture_stats,
            capture_not_ready,
            capture_more_data,
            capture_phase_misses,
            capture_packets_consumed);

        std::printf(
            "B5 worker %s failed: %s emergencySilence=%s log=%%TEMP%%\\B5_RUNTIME_FAILURE.txt\n",
            direction ? direction : "unknown",
            failure_message,
            silence_ok ? "OK" : "FAILED");
        return ERROR_GEN_FAILURE;
    };

    if (self->render_selected_ && self->capture_selected_) {
        // Render is the ASIO callback clock. Capture is an independent producer.
        // Do not hold a render callback waiting for an exact capture packet: at
        // 96 kHz the two WaveRT notification streams can have a stable phase
        // offset even though both packet sequences are continuous. Capture is
        // staged independently and the oldest unconsumed packet is presented at
        // the next render callback. Actual packet discontinuity remains fatal.
        HANDLE handles[3] = {
            self->stop_event_,
            self->capture_.notification_event(),
            self->render_.notification_event(),
        };

        alignas(64) std::uint8_t capture_stage
            [kNotificationCount][2][kMaxBufferFrames * kBytesPerAsioSample]{};
        bool capture_stage_valid[kNotificationCount] = {false, false};
        ULONG capture_stage_packet[kNotificationCount] = {0, 0};
        bool have_capture_consumed = false;
        ULONG last_capture_consumed = 0;
        ULONG consecutive_capture_misses = 0;

        auto stage_capture_packet = [&](ULONG capture_packet) -> bool {
            const ULONG slot = capture_packet % kNotificationCount;
            if (capture_stage_valid[slot] &&
                capture_stage_packet[slot] != capture_packet) {
                std::printf(
                    "B5 worker capture staging overrun new=%lu slot=%lu old=%lu\n",
                    capture_packet, slot, capture_stage_packet[slot]);
                return false;
            }

            if (!self->capture_.read_capture_packet24(
                    capture_packet,
                    capture_stage[slot][0],
                    capture_stage[slot][1],
                    static_cast<ULONG>(self->buffer_frames_))) {
                InterlockedIncrement(&self->capture_copy_errors_);
                return false;
            }

            capture_stage_valid[slot] = true;
            capture_stage_packet[slot] = capture_packet;
            return true;
        };

        auto drain_capture_available = [&]() -> int {
            // Return 1 when at least one packet was staged, 0 when no packet was
            // available, and -1 on a real capture failure.
            int staged = 0;
            BOOL more_data = FALSE;
            do {
                ULONG capture_packet = 0;
                more_data = FALSE;
                const X4WaveRtB5ProcessResult capture_result =
                    b5_mux_capture_event(self, &capture_packet, &more_data);

                if (capture_result == X4WaveRtB5ProcessResult::NoData) {
                    ++capture_not_ready;
                    break;
                }
                if (capture_result != X4WaveRtB5ProcessResult::Notification) {
                    return -1;
                }
                if (!stage_capture_packet(capture_packet)) {
                    return -1;
                }
                staged = 1;
                if (more_data) ++capture_more_data;
            } while (more_data &&
                     WaitForSingleObject(self->stop_event_, 0) != WAIT_OBJECT_0);
            return staged;
        };

        auto choose_capture_slot = [&]() -> int {
            if (!have_capture_consumed) {
                int selected = -1;
                for (ULONG slot = 0; slot < kNotificationCount; ++slot) {
                    if (!capture_stage_valid[slot]) continue;
                    if (selected < 0 ||
                        capture_stage_packet[slot] <
                            capture_stage_packet[static_cast<ULONG>(selected)]) {
                        selected = static_cast<int>(slot);
                    }
                }
                return selected;
            }

            const ULONG expected = last_capture_consumed + 1;
            const ULONG slot = expected % kNotificationCount;
            if (capture_stage_valid[slot] &&
                capture_stage_packet[slot] == expected) {
                return static_cast<int>(slot);
            }

            // A later staged packet without the expected predecessor would mean
            // the capture producer/consumer contract has been violated even if
            // the hardware packet counter itself remained sequential.
            for (ULONG other = 0; other < kNotificationCount; ++other) {
                if (capture_stage_valid[other] &&
                    capture_stage_packet[other] > expected) {
                    std::printf(
                        "B5 worker capture staging sequence mismatch expected=%lu staged=%lu\n",
                        expected, capture_stage_packet[other]);
                    return -2;
                }
            }
            return -1;
        };

        auto present_capture_for_render = [&](long buffer_index) -> bool {
            const int selected = choose_capture_slot();
            if (selected == -2) return false;

            const SIZE_T bytes =
                static_cast<SIZE_T>(self->buffer_frames_) * kBytesPerAsioSample;
            if (selected < 0) {
                ZeroMemory(self->input_buffers_[0][buffer_index], bytes);
                ZeroMemory(self->input_buffers_[1][buffer_index], bytes);
                ++capture_phase_misses;
                ++consecutive_capture_misses;
                if (consecutive_capture_misses > kB5CaptureStarvationLimit) {
                    std::printf(
                        "B5 worker capture starvation consecutive=%lu total=%lu\n",
                        consecutive_capture_misses, capture_phase_misses);
                    return false;
                }
                return true;
            }

            const ULONG slot = static_cast<ULONG>(selected);
            std::memcpy(
                self->input_buffers_[0][buffer_index],
                capture_stage[slot][0], bytes);
            std::memcpy(
                self->input_buffers_[1][buffer_index],
                capture_stage[slot][1], bytes);

            last_capture_consumed = capture_stage_packet[slot];
            have_capture_consumed = true;
            capture_stage_valid[slot] = false;
            ++capture_packets_consumed;
            consecutive_capture_misses = 0;
            return true;
        };

        for (;;) {
            const DWORD wait = WaitForMultipleObjects(3, handles, FALSE, 250);
            if (wait == WAIT_OBJECT_0) break;
            if (wait == WAIT_TIMEOUT) {
                return fail_worker("MUX", "notification timeout=250 ms");
            }
            if (wait != WAIT_OBJECT_0 + 1 && wait != WAIT_OBJECT_0 + 2) {
                return fail_worker("MUX", "WaitForMultipleObjects failed");
            }

            if (wait == WAIT_OBJECT_0 + 1) {
                if (drain_capture_available() < 0) {
                    return fail_worker("CAPTURE", self->capture_.last_message());
                }
                continue;
            }

            // A capture packet can become readable just before its auto-reset
            // event is observed. Opportunistically query once on every render
            // wake so output never waits for the capture notification phase.
            if (drain_capture_available() < 0) {
                return fail_worker("CAPTURE", self->capture_.last_message());
            }

            ULONG render_packet = 0;
            if (!b5_mux_render_event(self, &render_packet)) {
                return fail_worker("RENDER", self->render_.last_message());
            }

            const long buffer_index = static_cast<long>(
                (render_packet + 1) % kNotificationCount);
            if (!present_capture_for_render(buffer_index)) {
                return fail_worker(
                    "DUPLEX", "capture staging/starvation failure");
            }
            if (!b5_mux_dispatch_callback(
                    self, render_packet, buffer_index)) {
                return fail_worker(
                    "DUPLEX", "callback/render copy failed");
            }
        }
    } else if (self->render_selected_) {
        HANDLE handles[2] = {
            self->stop_event_,
            self->render_.notification_event(),
        };
        for (;;) {
            const DWORD wait = WaitForMultipleObjects(2, handles, FALSE, 250);
            if (wait == WAIT_OBJECT_0) break;
            if (wait == WAIT_TIMEOUT) {
                return fail_worker("RENDER", "notification timeout=250 ms");
            }
            if (wait != WAIT_OBJECT_0 + 1) {
                return fail_worker("RENDER", "notification wait failed");
            }

            ULONG render_packet = 0;
            if (!b5_mux_render_event(self, &render_packet)) {
                return fail_worker("RENDER", self->render_.last_message());
            }
            const long buffer_index = static_cast<long>(
                (render_packet + 1) % kNotificationCount);
            if (!b5_mux_dispatch_callback(self, render_packet, buffer_index)) {
                return fail_worker("RENDER", "callback/render copy failed");
            }
        }
    } else if (self->capture_selected_) {
        HANDLE handles[2] = {
            self->stop_event_,
            self->capture_.notification_event(),
        };
        for (;;) {
            const DWORD wait = WaitForMultipleObjects(2, handles, FALSE, 250);
            if (wait == WAIT_OBJECT_0) break;
            if (wait == WAIT_TIMEOUT) {
                return fail_worker("CAPTURE", "notification timeout=250 ms");
            }
            if (wait != WAIT_OBJECT_0 + 1) {
                return fail_worker("CAPTURE", "notification wait failed");
            }

            BOOL more_data = FALSE;
            do {
                ULONG capture_packet = 0;
                more_data = FALSE;
                const X4WaveRtB5ProcessResult capture_result =
                    b5_mux_capture_event(
                        self, &capture_packet, &more_data);

                if (capture_result == X4WaveRtB5ProcessResult::NoData) {
                    ++capture_not_ready;
                    break;
                }
                if (capture_result != X4WaveRtB5ProcessResult::Notification) {
                    return fail_worker("CAPTURE", self->capture_.last_message());
                }

                const long buffer_index = static_cast<long>(
                    capture_packet % kNotificationCount);
                if (!self->capture_.read_capture_packet24(
                        capture_packet,
                        self->input_buffers_[0][buffer_index],
                        self->input_buffers_[1][buffer_index],
                        static_cast<ULONG>(self->buffer_frames_))) {
                    InterlockedIncrement(&self->capture_copy_errors_);
                    return fail_worker("CAPTURE", self->capture_.last_message());
                }
                if (!b5_mux_dispatch_callback(
                        self, capture_packet, buffer_index)) {
                    return fail_worker("CAPTURE", "callback failed");
                }
                if (more_data) ++capture_more_data;
            } while (more_data &&
                     WaitForSingleObject(self->stop_event_, 0) != WAIT_OBJECT_0);
        }
    }

    std::printf(
        "B5 worker EXIT adapter=%s thread=%lu captureNotReady=%lu captureMoreData=%lu capturePhaseMisses=%lu captureConsumed=%lu\n",
        kB5MuxAdapter, GetCurrentThreadId(), capture_not_ready,
        capture_more_data, capture_phase_misses, capture_packets_consumed);
    return ERROR_SUCCESS;
}

DWORD WINAPI b5_mux_thread_entry(LPVOID parameter) {
    auto* self = static_cast<X4AsioDriverB5*>(parameter);
    if (!self) return ERROR_INVALID_PARAMETER;

    DWORD task_index = 0;
    HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);
    const DWORD mmcss_error = mmcss ? ERROR_SUCCESS : GetLastError();
    BOOL priority_ok = FALSE;
    if (mmcss) {
        priority_ok = AvSetMmThreadPriority(mmcss, AVRT_PRIORITY_CRITICAL);
    } else {
        priority_ok = SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    }

    std::printf(
        "B5 worker realtime adapter=%s failsafe=%s mmcss=%s priority=%s taskIndex=%lu error=%lu thread=%lu\n",
        kB5MuxAdapter,
        kB5RuntimeFailsafe,
        mmcss ? "Pro Audio" : "FALLBACK",
        priority_ok ? "OK" : "FAIL",
        task_index, mmcss_error, GetCurrentThreadId());

    const DWORD result = b5_mux_worker_loop(self);
    if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
    std::fflush(stdout);
    return result;
}

HANDLE WINAPI b5_create_mux_thread(
    LPSECURITY_ATTRIBUTES thread_attributes,
    SIZE_T stack_size,
    LPTHREAD_START_ROUTINE original_start_routine,
    LPVOID parameter,
    DWORD creation_flags,
    LPDWORD thread_id) {

    (void)original_start_routine;
    return ::CreateThread(
        thread_attributes,
        stack_size,
        &b5_mux_thread_entry,
        parameter,
        creation_flags,
        thread_id);
}

} // namespace

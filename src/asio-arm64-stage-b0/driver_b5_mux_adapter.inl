// B5 high-rate worker adapter.
//
// Included by the ARM64EC and Classic ARM64 driver translation units after
// driver_b5.cpp. The driver class is translation-unit-local and exposed only
// to this adapter; WaveRT engine internals remain private and are reached only
// through the public signaled-notification API.

namespace {

bool b5_mux_copy_capture(X4AsioDriverB5* self, ULONG packet_number) {
    const long buffer_index = static_cast<long>(packet_number % kNotificationCount);
    if (!self->capture_.read_capture_packet24(
            packet_number,
            self->input_buffers_[0][buffer_index],
            self->input_buffers_[1][buffer_index],
            static_cast<ULONG>(self->buffer_frames_))) {
        InterlockedIncrement(&self->capture_copy_errors_);
        return false;
    }
    return true;
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
        "B5 worker START adapter=dual-event-mux-v2 thread=%lu rate=%.0f frames=%ld render=%d capture=%d\n",
        GetCurrentThreadId(), self->sample_rate_, self->buffer_frames_,
        self->render_selected_ ? 1 : 0, self->capture_selected_ ? 1 : 0);

    ULONG capture_not_ready = 0;
    ULONG capture_more_data = 0;

    auto fail_worker = [&](const char* direction, const char* message) -> DWORD {
        InterlockedExchange(&self->worker_failed_, 1);
        std::printf("B5 worker %s failed: %s\n",
                    direction, message ? message : "unknown");
        return ERROR_GEN_FAILURE;
    };

    if (self->render_selected_ && self->capture_selected_) {
        // Capture has the lower wait index intentionally. If both auto-reset
        // events are signaled, consume input first so capture cannot be starved
        // by the render cadence at 96 kHz.
        HANDLE handles[3] = {
            self->stop_event_,
            self->capture_.notification_event(),
            self->render_.notification_event(),
        };

        bool capture_slot_valid[kNotificationCount] = {false, false};
        ULONG capture_slot_packet[kNotificationCount] = {0, 0};
        bool pending_render = false;
        ULONG pending_render_packet = 0;

        auto try_dispatch_pending = [&]() -> bool {
            if (!pending_render) return true;
            if (pending_render_packet == 0) return false;

            const ULONG expected_capture = pending_render_packet - 1;
            const ULONG slot = expected_capture % kNotificationCount;
            if (!capture_slot_valid[slot]) return true;
            if (capture_slot_packet[slot] < expected_capture) return true;
            if (capture_slot_packet[slot] != expected_capture) {
                std::printf(
                    "B5 worker duplex sync mismatch render=%lu expectedCapture=%lu slotCapture=%lu\n",
                    pending_render_packet, expected_capture, capture_slot_packet[slot]);
                return false;
            }

            const long buffer_index = static_cast<long>(
                (pending_render_packet + 1) % kNotificationCount);
            if (buffer_index != static_cast<long>(slot)) {
                std::printf(
                    "B5 worker duplex parity mismatch render=%lu capture=%lu buffer=%ld slot=%lu\n",
                    pending_render_packet, expected_capture, buffer_index, slot);
                return false;
            }

            if (!b5_mux_dispatch_callback(
                    self, pending_render_packet, buffer_index)) {
                return false;
            }

            capture_slot_valid[slot] = false;
            pending_render = false;
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
                    if (!b5_mux_copy_capture(self, capture_packet)) {
                        return fail_worker("CAPTURE", self->capture_.last_message());
                    }

                    const ULONG slot = capture_packet % kNotificationCount;
                    if (capture_slot_valid[slot] &&
                        capture_slot_packet[slot] != capture_packet) {
                        return fail_worker(
                            "DUPLEX",
                            "capture cyclic slot would overwrite an unconsumed packet");
                    }
                    capture_slot_valid[slot] = true;
                    capture_slot_packet[slot] = capture_packet;

                    if (!try_dispatch_pending()) {
                        return fail_worker(
                            "DUPLEX", "capture/render synchronization failed");
                    }

                    if (more_data) ++capture_more_data;
                } while (more_data &&
                         WaitForSingleObject(self->stop_event_, 0) != WAIT_OBJECT_0);
            } else {
                if (pending_render) {
                    return fail_worker(
                        "DUPLEX",
                        "next render notification arrived before prior capture synchronization");
                }

                ULONG render_packet = 0;
                if (!b5_mux_render_event(self, &render_packet)) {
                    return fail_worker("RENDER", self->render_.last_message());
                }
                pending_render = true;
                pending_render_packet = render_packet;
                if (!try_dispatch_pending()) {
                    return fail_worker(
                        "DUPLEX", "render/capture synchronization failed");
                }
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
                if (!b5_mux_copy_capture(self, capture_packet)) {
                    return fail_worker("CAPTURE", self->capture_.last_message());
                }

                const long buffer_index = static_cast<long>(
                    capture_packet % kNotificationCount);
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
        "B5 worker EXIT adapter=dual-event-mux-v2 thread=%lu captureNotReady=%lu captureMoreData=%lu\n",
        GetCurrentThreadId(), capture_not_ready, capture_more_data);
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
        "B5 worker realtime adapter=dual-event-mux-v2 mmcss=%s priority=%s taskIndex=%lu error=%lu thread=%lu\n",
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

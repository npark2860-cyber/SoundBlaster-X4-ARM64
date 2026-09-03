#pragma once

#include <windows.h>
#include <cstdint>

struct X4WaveRtStats {
    ULONG notifications = 0;
    ULONG packet_discontinuities = 0;
    ULONG position_regressions = 0;
    ULONG hardware_buffer_writes = 0;
    ULONG dma_frames_copied = 0;
    ULONG dma_nonzero_samples = 0;
    ULONG last_write_packet = 0;
};

enum class X4WaveRtPrepareResult {
    Ready,
    Busy,
    Indeterminate,
    Failed,
};

using X4WaveRtNotificationObserver = bool (*)(
    void* context,
    ULONG zero_based_notification_index,
    ULONG packet_count);

class X4WaveRtEngine {
public:
    X4WaveRtEngine() = default;
    ~X4WaveRtEngine();

    X4WaveRtEngine(const X4WaveRtEngine&) = delete;
    X4WaveRtEngine& operator=(const X4WaveRtEngine&) = delete;

    X4WaveRtPrepareResult prepare();
    bool start_and_observe(
        X4WaveRtNotificationObserver observer = nullptr,
        void* observer_context = nullptr);
    bool write_interleaved_packet(
        ULONG absolute_packet_number,
        const std::int16_t* left,
        const std::int16_t* right,
        ULONG frames);
    bool stop();
    void dispose();

    bool prepared() const { return prepared_; }
    bool running() const { return entered_run_; }
    const X4WaveRtStats& stats() const { return stats_; }
    const char* last_message() const { return last_message_; }

private:
    HANDLE filter_ = INVALID_HANDLE_VALUE;
    HANDLE pin_ = INVALID_HANDLE_VALUE;
    HANDLE event_ = nullptr;
    void* buffer_address_ = nullptr;
    ULONG actual_buffer_size_ = 0;
    bool call_memory_barrier_ = false;
    bool notification_registered_ = false;
    bool prepared_ = false;
    bool entered_run_ = false;
    X4WaveRtStats stats_{};
    char last_message_[192] = "Stage B3B WaveRT engine not prepared";
};

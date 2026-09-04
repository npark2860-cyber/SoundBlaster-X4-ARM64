#pragma once

#include <windows.h>
#include <cstdint>

struct X4WaveRtB5Stats {
    ULONG notifications = 0;
    ULONG packet_discontinuities = 0;
    ULONG position_regressions = 0;
    ULONG hardware_buffer_writes = 0;
    ULONG dma_frames_copied = 0;
    ULONG dma_nonzero_samples = 0;
    ULONG last_packet = 0;
};

enum class X4WaveRtB5PrepareResult {
    Ready,
    Busy,
    Indeterminate,
    Failed,
};

enum class X4WaveRtB5ProcessResult {
    Notification,
    NoData,
    StopRequested,
    Failed,
};

enum class X4WaveRtB5Direction {
    Render,
    Capture,
};

struct X4WaveRtB5Config {
    X4WaveRtB5Direction direction = X4WaveRtB5Direction::Render;
    ULONG pin_id = 1;
    ULONG sample_rate = 48000;
    WORD channels = 2;
    WORD bits_per_sample = 24;
    ULONG frames_per_packet = 240;
    ULONG notification_count = 2;
};

using X4WaveRtB5NotificationObserver = bool (*)(
    void* context,
    ULONG zero_based_notification_index,
    ULONG packet_number);

class X4WaveRtEngineB5 {
public:
    X4WaveRtEngineB5() = default;
    ~X4WaveRtEngineB5();

    X4WaveRtEngineB5(const X4WaveRtEngineB5&) = delete;
    X4WaveRtEngineB5& operator=(const X4WaveRtEngineB5&) = delete;

    X4WaveRtB5PrepareResult prepare(const X4WaveRtB5Config& config);
    bool start_run();

    X4WaveRtB5ProcessResult process_one_notification(
        HANDLE stop_event,
        DWORD timeout_ms,
        X4WaveRtB5NotificationObserver observer = nullptr,
        void* observer_context = nullptr);

    // B5 high-rate worker API: the caller has already observed this engine's
    // notification_event() as signaled. No additional wait occurs here.
    // Capture ERROR_NOT_READY is reported as NoData, not as a hard failure.
    X4WaveRtB5ProcessResult process_signaled_notification(
        ULONG* packet_number,
        BOOL* more_data,
        X4WaveRtB5NotificationObserver observer = nullptr,
        void* observer_context = nullptr,
        bool trace_notification = false);

    bool write_render_packet24(
        ULONG absolute_packet_number,
        const std::uint8_t* left,
        const std::uint8_t* right,
        ULONG frames);

    bool read_capture_packet24(
        ULONG absolute_packet_number,
        std::uint8_t* left,
        std::uint8_t* right,
        ULONG frames);

    bool stop();
    void dispose();

    bool prepared() const { return prepared_; }
    bool running() const { return entered_run_; }
    HANDLE notification_event() const { return event_; }
    const X4WaveRtB5Config& config() const { return config_; }
    const X4WaveRtB5Stats& stats() const { return stats_; }
    const char* last_message() const { return last_message_; }

private:
    HANDLE filter_ = INVALID_HANDLE_VALUE;
    HANDLE pin_ = INVALID_HANDLE_VALUE;
    HANDLE event_ = nullptr;
    void* buffer_address_ = nullptr;
    ULONG actual_buffer_size_ = 0;
    ULONG packet_bytes_ = 0;
    ULONG bytes_per_frame_ = 0;
    bool call_memory_barrier_ = false;
    bool notification_registered_ = false;
    bool prepared_ = false;
    bool entered_run_ = false;
    bool have_previous_packet_ = false;
    ULONG previous_packet_ = 0;
    UINT64 previous_position_ = 0;
    bool have_previous_position_ = false;
    X4WaveRtB5Config config_{};
    X4WaveRtB5Stats stats_{};
    char last_message_[224] = "B5 WaveRT engine not prepared";
};

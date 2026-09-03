#pragma once

#include <windows.h>

struct X4WaveRtStats {
    ULONG notifications = 0;
    ULONG packet_discontinuities = 0;
    ULONG position_regressions = 0;
};

enum class X4WaveRtPrepareResult {
    Ready,
    Busy,
    Indeterminate,
    Failed,
};

class X4WaveRtEngine {
public:
    X4WaveRtEngine() = default;
    ~X4WaveRtEngine();

    X4WaveRtEngine(const X4WaveRtEngine&) = delete;
    X4WaveRtEngine& operator=(const X4WaveRtEngine&) = delete;

    X4WaveRtPrepareResult prepare();
    bool start_and_observe();
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
    bool notification_registered_ = false;
    bool prepared_ = false;
    bool entered_run_ = false;
    X4WaveRtStats stats_{};
    char last_message_[192] = "Stage B2 WaveRT engine not prepared";
};

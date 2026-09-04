#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdlib.h>

struct B5ControlPanelState {
    double sample_rate = 48000.0;
    long active_buffer_frames = 0;
    bool buffers_created = false;
    bool worker_running = false;
    char last_status[124]{};
};

// Returns the sanitized per-user preference for the selected rate. Missing or
// invalid values fall back to the measured B5 defaults (240 at 48/96 kHz,
// 384 at 192 kHz). The 512-frame compatibility exception remains valid.
long b5_load_preferred_buffer_frames(double sample_rate);

// Native Win32 control panel. This function only consumes the supplied driver
// snapshot and HKCU preferences; it never opens/probes WaveRT hardware.
// Returns false only when the panel itself could not be created.
bool b5_show_control_panel(
    HINSTANCE module,
    HWND owner_window,
    const B5ControlPanelState& state);

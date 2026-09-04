# NEXT ACTION — B5 Native ASIO Control Panel

Updated: 2026-09-04 KST

## Goal

Implement the first-release native Win32 control panel for `Sound Blaster X4 ARM64 ASIO B5` without disturbing the validated streaming path.

This document is intentionally UI/product-surface focused. Runtime WaveRT notification work is a separate track.

---

## Source state

Repository:

`npark2860-cyber/SoundBlaster-X4-ARM64`

Validated B5 runtime reference:

`ca37f0e8427227733cd6082a50e20101312e3333`

Evidence attached to that built bundle:

- full B5 product matrix PASS
- REAPER-matched 48 kHz / 480 frames / 5 seconds: 500 callbacks, stop=0, workerJoined=YES
- user subsequently reported ordinary REAPER playback had no problem

Current productization branch:

`exp/windows-arm64-asio-b5-capability-productization@4475fc17b70f372fe317fa201f201e8dc5543f9f`

Latest runtime-code commit on it:

`64e34b48714789ab17fba57be34b054f2170b4e9`

That newer post-coalesce stale-wake delta has not yet been built or runtime-validated. Do not silently treat the branch HEAD as the validated streaming baseline.

Preferred UI isolation branch:

`exp/windows-arm64-asio-b5-control-panel`

Before creating or editing it, verify current refs and show the intended base. Prefer the validated `ca37f0e...` runtime reference for the UI branch so the control-panel work does not accidentally depend on an unvalidated worker delta.

---

## Relevant current code facts

Primary source:

`src/asio-arm64-stage-b0/driver_b5.cpp`

Current control-panel entry point:

```cpp
ASIOError controlPanel() override { return ASE_NotPresent; }
```

Useful existing state:

- `HMODULE g_module` exists
- `DllMain(DLL_PROCESS_ATTACH)` stores the module handle in `g_module`
- `init(void* sysHandle)` currently discards `sysHandle`
- `sample_rate_` stores current ASIO sample rate
- `buffer_frames_` stores the host-selected buffer after `createBuffers()`
- `buffers_created_`, worker/pin RUN state already exist and must gate unsafe changes
- `callbacks_` becomes valid in `createBuffers()` and is cleared by buffer disposal
- `getLatencies()` reports active `buffer_frames_` if buffers exist, otherwise rate-specific preferred value
- `getBufferSize()` currently reports fixed rate-specific min/max/preferred/granularity
- `setSampleRate()` already rejects changes while buffers/RUN are active
- `createBuffers()` validates the host-supplied buffer size and rejects already-created/prepared state

Do not change these streaming contracts merely to make the UI convenient.

---

## ASIO buffer semantic constraint

The host passes the actual `bufferSize` into `createBuffers()`.

Therefore a panel-selected latency/buffer preference must not be implemented by silently replacing the explicit host argument inside `createBuffers()`.

The panel needs a clear contract such as:

- display current/effective buffer separately from preferred/next-open buffer;
- persist a next-safe-open preference;
- expose that preference through the appropriate preferred-buffer path only if doing so is ASIO-correct;
- if a host reset/reopen request is required, implement it deliberately rather than mutating live geometry.

Before using `callbacks_.asioMessage(kAsioResetRequest, ...)`, verify:

- callbacks are currently valid;
- controlPanel call context / reentrancy expectations;
- no teardown is in progress;
- request is sent only after the dialog/update state is consistent.

Do not introduce a reset callback just because Creative's panel appears to update immediately.

---

## UI requirements

The user explicitly wants something that looks credible rather than a bare debug dialog.

Use own native Windows UI. Do not reuse Creative UI binaries or resources.

Recommended visual/product structure:

### Header

- `Sound Blaster X4 ARM64 ASIO B5`
- small driver/version line
- compact status indicator/text

### Audio status

- Sample Rate: current effective value, e.g. `48,000 Hz`
- Current Buffer: active host buffer if created, e.g. `480 samples`
- Effective Latency: frames and milliseconds, e.g. `480 samples / 10.00 ms`

### Latency / Buffer Size

Primary editable control.

48/96 kHz contract:

- min 96
- max 4800
- granularity 48
- preferred/default 240
- 512 compatibility value accepted

192 kHz contract:

- min 384
- max 4800
- granularity 48
- preferred/default 384
- 512 compatibility accepted

Do not label 240 as `Current` when the host has 480 active. Keep `Current/Effective` and `Preferred/Next Open` concepts visibly distinct if both are shown.

A combo box/list or slider+numeric display is acceptable, but arbitrary invalid values must not be accepted.

### Apply behavior

- Cancel: no persisted or runtime change
- Apply: persist selected next-safe-open preference; keep dialog open
- OK: apply same validated state then close
- if buffers/RUN are active, do not live-reallocate WaveRT
- clearly indicate if reopening/reset is required

### Diagnostics

Keep a lightweight diagnostics section because runtime debugging is a product requirement.

Useful first-release fields/actions:

- driver version
- current sample rate
- active/effective buffer
- buffers created / worker running state
- last driver status/error string
- fatal runtime log location: `%TEMP%\B5_RUNTIME_FAILURE.txt`
- `Copy Diagnostics` or `Save Diagnostic Report`

Do not write a file every callback. Existing fatal logging remains failure-only.

---

## Persistence

Use per-user persistence; do not require admin merely to change latency preference.

A small HKCU key or equivalent Windows-native per-user store is appropriate. Keep scope limited to B5 control-panel settings.

Persist only validated user settings such as preferred/next-open buffer. Do not persist transient worker/hardware state.

On load:

- sanitize stored value against the current sample-rate contract;
- if invalid/missing, fall back to 240 at 48/96 or 384 at 192;
- preserve 512 compatibility explicitly.

Do not silently coerce an active host buffer to the stored preference.

---

## Owner window / modal behavior

`init(void* sysHandle)` currently discards the system handle.

Before using it as an HWND owner, verify the ASIO Windows host convention in the SDK/reference used by this repository. If confirmed, retain it safely as a non-owning owner-window reference.

If it cannot be verified immediately, a correct unowned modal panel is preferable to inventing an HWND assumption.

No hardware pin must be opened merely because the dialog opens.

---

## Architecture / build constraints

Panel must build in both:

- ARM64EC B5 DLL
- Classic ARM64 B5 DLL

Prefer shared source used by both adapter builds.

Avoid external UI runtimes or heavyweight dependencies. Native Win32 is preferred.

If a resource script complicates ARM64EC/Classic sharing, a runtime-created Win32 window/control hierarchy is acceptable. The UI still needs deliberate spacing, typography, alignment and state feedback.

No unrelated build-system cleanup.

---

## Runtime code that must remain untouched unless concrete evidence requires otherwise

Do not refactor for style:

- BUSY/pre-pin ownership gates
- WaveRT render/capture engine
- mux worker policy
- joined-worker shutdown sequence
- `runtime-failsafe-v1`
- packet discontinuity / position regression / callback-index / copy checks

The control panel is not a reason to modify notification timing.

---

## First implementation sequence

1. verify GitHub main and B5 refs;
2. read `CURRENT_HANDOFF.md` and this file fully;
3. inspect validated `ca37f0e` `driver_b5.cpp`, CMake targets, ARM64EC adapter and Classic ARM64 adapter;
4. show proposed control-panel branch/base and exact files to touch;
5. create the dedicated branch only after the base is clear;
6. add UI state/persistence in small isolated source files where possible;
7. replace `ASE_NotPresent` with native panel invocation;
8. do not implement host reset until callback lifetime/reentrancy is understood;
9. build ARM64EC + Classic ARM64;
10. validate panel open/close/cancel before testing setting application;
11. verify panel open creates no WaveRT pins;
12. validate persistence and active-state rejection;
13. only then consider a reset/reopen notification if needed.

---

## Acceptance criteria

Panel milestone is PASS when:

- REAPER/another ASIO host can open the B5 panel reliably;
- panel looks like a compact production audio-driver settings panel;
- effective sample rate and active buffer are truthful;
- selectable buffers obey B5's measured contracts;
- current active buffer is never confused with preferred/default;
- no WaveRT pin is created by opening the panel;
- no active stream geometry is mutated in place;
- Cancel is a true no-op;
- Apply/OK persistence is deterministic;
- invalid stored settings are sanitized;
- ARM64EC + Classic ARM64 both build;
- validated B5 streaming code is not refactored or weakened;
- diagnostics remain available without realtime-path file I/O.

After this passes, run the standard B5 product regression and one ordinary REAPER test. The separate post-coalesce stale-wake runtime patch can then be reconciled/validated explicitly rather than being hidden inside UI work.

# NEXT ACTION — B5 Native ASIO Control Panel

Updated: 2026-09-04 KST

## Milestone state

The first-pass native B5 control panel is implemented and currently appears stable in user testing.

Control-panel branch:

`exp/windows-arm64-asio-b5-control-panel@7bc83f87b172f574064d086affe5e8ed1d6fbdff`

Validated runtime integration base:

`exp/windows-arm64-asio-b5-capability-productization@4475fc17b70f372fe317fa201f201e8dc5543f9f`

The control-panel branch differs from that base in exactly five source/build paths:

- `src/asio-arm64-stage-b0/CMakeLists.txt`
- `src/asio-arm64-stage-b0/control_panel_b5.cpp`
- `src/asio-arm64-stage-b0/control_panel_b5.h`
- `src/asio-arm64-stage-b0/driver_b5.cpp`
- `src/asio-arm64-stage-b5-classic/CMakeLists.txt`

Do not expand this scope casually.

---

## Implemented behavior

- own native Win32 UI
- no Creative binary/resource reuse
- `IASIO::controlPanel()` entry point
- product identity `Sound Blaster X4 ARM64 ASIO B5`
- current sample-rate display
- current/effective buffer display when buffers exist
- frames + milliseconds latency display
- per-rate preferred/next-open buffer selection
- 48/96 kHz: 96..4800, step 48, 512 compatibility
- 192 kHz: 384..4800, step 48, 512 compatibility
- Apply / OK / Cancel
- per-user HKCU persistence
- host `createBuffers(bufferSize)` remains authoritative
- no live geometry mutation
- no `kAsioResetRequest`
- lightweight diagnostics + Copy
- no realtime callback-path file writes

Persistence key:

`HKCU\Software\SoundBlaster-X4-ARM64\ASIO B5`

Values:

- `PreferredBufferFrames48_96`
- `PreferredBufferFrames192`
- `SampleRate`

---

## Runtime evidence already obtained

ARM64EC / REAPER:

- panel opens successfully;
- user selected/applied 96,000 Hz in REAPER;
- after ASIO reopen/reinitialization, panel still displayed `96,000 Hz`;
- the prior forced `sample_rate_ = 48000.0` reset in `init()` is therefore no longer reproducing;
- user currently reports no apparent problem with the panel.

Observed 96 kHz panel snapshot:

- Sample Rate: `96,000 Hz`
- Preferred for next open: `480 samples / 5.00 ms`
- Current Buffer: `Not active`
- Effective Latency: `Not active`

Do not overclaim active-buffer display runtime validation from this screenshot; buffers were not active at the snapshot.

---

## Latest cosmetic fix

The original Unicode bullet in the version line rendered as mojibake in the user's Windows UI.

Changed:

`Driver 2.00  •  ARM64EC`

To:

`Driver 2.00  |  ARM64EC`

This is a one-line UI-only source change. ARM64EC and Classic ARM64 were both rebuilt successfully afterward.

Latest build:

- Run `33854864840`
- Artifact `SoundBlaster-X4-ASIO-B5-Control-Panel-ASCII-Fix`
- Artifact ID `9929782504`
- SHA-256 `ade6c7b7ddfeff9a462e6bef4b55c1fdcd359db2228f815956d3450319aa964a`

---

## Architecture status

ARM64EC:

- compile/link PASS
- panel invoked in REAPER
- 96 kHz persistence user-confirmed

Classic ARM64:

- compile/link PASS
- runtime marker checks PASS
- panel runtime not yet separately exercised

Do not imply Classic runtime verification merely from shared source or successful build.

---

## Runtime code freeze

The control-panel branch must continue to preserve the validated B5 runtime path.

Do not modify from this track:

- BUSY / pin-instance gates
- WaveRT render/capture engine
- mux worker
- joined-worker teardown
- `runtime-failsafe-v1`
- `post-coalesce-stale-v1`
- strict packet / position / callback-index / copy checks

The separate 192k/384 long-cadence `delta=3` issue remains outside control-panel scope.

---

## What to do next

Default action: **freeze the control panel unless a concrete problem is observed.**

Only perform additional targeted panel validation if it serves a specific question. Useful remaining checks, if later needed:

1. confirm the ASCII `|` separator visually on the latest DLL;
2. observe the panel while host buffers are genuinely still created, to verify current/effective buffer reporting on the active path;
3. exercise Classic ARM64 panel runtime if a Classic-only host use case matters.

Do not rerun old audible REAPER tests, product geometry probes, or broad runtime matrices merely because the panel changed cosmetically.

If no new panel issue appears, return to the separate B5 runtime closure item: 192 kHz / 384-frame long-cadence forward `delta=3`.

---

## Acceptance wording

Current milestone can be described as:

**Native B5 control panel implemented; ARM64EC REAPER open and 96 kHz persistence user-confirmed; ARM64EC/Classic builds pass; no apparent panel issue reported in current use.**

Do not yet describe the entire B5 first release as fully frozen because the separate 192k/384 runtime issue remains open.

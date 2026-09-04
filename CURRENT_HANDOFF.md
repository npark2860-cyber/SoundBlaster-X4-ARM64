# CURRENT HANDOFF — Sound Blaster X4 Windows ARM64

Updated: 2026-09-04 KST

## Source of truth

Repository:

`npark2860-cyber/SoundBlaster-X4-ARM64`

GitHub is source of truth. At the start of a later chat, verify actual refs again instead of reconstructing state from old conversation context.

Current B5 productization runtime base:

`exp/windows-arm64-asio-b5-capability-productization@4475fc17b70f372fe317fa201f201e8dc5543f9f`

Current B5 control-panel branch:

`exp/windows-arm64-asio-b5-control-panel@7bc83f87b172f574064d086affe5e8ed1d6fbdff`

The control-panel branch is directly based on the validated productization runtime and differs from `4475fc17...` in exactly five source/build paths:

- `src/asio-arm64-stage-b0/CMakeLists.txt`
- `src/asio-arm64-stage-b0/control_panel_b5.cpp`
- `src/asio-arm64-stage-b0/control_panel_b5.h`
- `src/asio-arm64-stage-b0/driver_b5.cpp`
- `src/asio-arm64-stage-b5-classic/CMakeLists.txt`

No WaveRT engine, mux adapter, B4D, preflight, or runtime-failsafe source is modified by the control-panel branch.

---

# Read order

1. `CURRENT_HANDOFF.md`
2. `NEXT_ACTION_ASIO_CONTROL_PANEL.md`
3. `NEXT_ACTION_ASIO.md`
4. `DEBUG_HISTORY_20260904_ASIO_B5_POST_COALESCE_STALE_WAKE.md`
5. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V4_STATS_ALIAS_REGRESSION.md`
6. older histories only when needed

CTCDC/CTIntrfu remains separate from the current ASIO control-panel stabilization work.

---

# B5 runtime state to preserve

The current integration base already contains the validated runtime protections and marker set:

- `dual-event-mux-v4-coalesce-recovery`
- `runtime-failsafe-v1`
- `packet-stats-observed-v1`
- `post-coalesce-stale-v1`

The control-panel build rechecked those markers in both ARM64EC and Classic ARM64 outputs.

Do not alter or weaken:

- BUSY / immutable pin-instance gates
- WaveRT Render Pin 1 / Capture Pin 4 engine
- mux worker policy
- joined-worker teardown safety
- packet discontinuity / position regression / callback-index / copy checks
- `runtime-failsafe-v1`
- `post-coalesce-stale-v1`

The separate long-cadence 192 kHz / 384-frame `delta=3` issue remains a runtime follow-up. Do not solve or weaken it from control-panel work.

---

# Native B5 control panel — implemented state

The first native panel is implemented with own Win32 code. It does not reuse Creative binaries or Creative UI resources.

Shared panel source:

- `src/asio-arm64-stage-b0/control_panel_b5.cpp`
- `src/asio-arm64-stage-b0/control_panel_b5.h`

Both ARM64EC and Classic ARM64 builds consume the same shared control-panel implementation.

Implemented behavior:

- opened through `IASIO::controlPanel()`
- product identity `Sound Blaster X4 ARM64 ASIO B5`
- compact native Win32 UI
- current ASIO sample-rate display
- current/effective buffer display when host buffers exist
- effective latency shown as frames + milliseconds
- rate-specific next-open buffer list
- 48/96 kHz: 96..4800, granularity 48, 512 compatibility
- 192 kHz: 384..4800, granularity 48, 512 compatibility
- Apply saves preference and keeps dialog open
- OK saves preference and closes
- Cancel closes without saving the current unapplied selection
- no live WaveRT geometry mutation
- no hidden override of host `createBuffers(bufferSize)`
- no `kAsioResetRequest`
- lightweight diagnostics + Copy action
- no callback-path file I/O added
- existing `%TEMP%\B5_RUNTIME_FAILURE.txt` remains failure-only

Per-user persistence key:

`HKCU\Software\SoundBlaster-X4-ARM64\ASIO B5`

Values currently used:

- `PreferredBufferFrames48_96`
- `PreferredBufferFrames192`
- `SampleRate`

---

# Sample-rate persistence fix

The original panel build exposed a real state bug: `init()` forced `sample_rate_ = 48000.0` every time ASIO was reinitialized.

That reset has been removed.

Current behavior:

- successful `setSampleRate(48000/96000/192000)` stores the selected rate in HKCU;
- a later `init()` restores that persisted supported rate;
- invalid/missing persistence falls back to 48 kHz;
- the ASIO host remains authoritative when it explicitly calls `setSampleRate()`.

User runtime confirmation in REAPER / ARM64EC:

- REAPER was set to 96,000 Hz and applied;
- after reopening/reinitializing the ASIO device/panel, the panel still displayed `96,000 Hz`;
- the user reported that the panel currently appears to have no problem.

The observed panel at 96 kHz also showed:

- `Preferred for next open: 480 samples / 5.00 ms`
- `Current Buffer: Not active`
- `Effective Latency: Not active`

`Current Buffer: Not active` means the panel-open snapshot did not have host buffers created at that moment. Do not treat this screenshot as proof of the active-buffer display path.

---

# UI encoding cleanup

The first runtime screenshot showed the separator bullet rendered as mojibake:

`Driver 2.00 â€¢ ARM64EC`

Source was changed from the Unicode bullet to an ASCII separator:

`Driver 2.00  |  ARM64EC`

That one-line change was rebuilt successfully for ARM64EC and Classic ARM64. The latest ASCII separator build has not yet been separately visually reconfirmed by the user, but compile/link and marker checks passed.

---

# Build / artifact evidence

Latest control-panel build with the sample-rate persistence and ASCII separator fixes:

GitHub Actions run:

`33854864840`

Artifact:

`SoundBlaster-X4-ASIO-B5-Control-Panel-ASCII-Fix`

Artifact ID:

`9929782504`

SHA-256:

`ade6c7b7ddfeff9a462e6bef4b55c1fdcd359db2228f815956d3450319aa964a`

The artifact contains only the two DLL outputs:

- ARM64EC B5 DLL
- Classic ARM64 B5 DLL

Build validation passed for both architectures, including runtime marker checks.

Runtime/UI interaction has been exercised on ARM64EC in REAPER. Classic ARM64 remains build/link verified but is not yet separately panel-runtime verified.

---

# Current milestone status

Use the following wording precisely:

- **Control-panel implementation: complete for the current first-pass feature set.**
- **ARM64EC REAPER panel open + 96 kHz persistence: user-confirmed.**
- **ARM64EC and Classic ARM64 compile/link: PASS.**
- **Latest validated B5 runtime protections remain present.**
- **User currently reports no apparent issue in normal panel use.**
- **Do not call the entire B5 first release fully frozen yet.**

Remaining validation gaps are narrow and should not trigger broad retesting unless needed:

- active-buffer display while host buffers are actually still created has not been directly observed in the current screenshot;
- Classic ARM64 panel runtime has not been separately exercised;
- ASCII separator change has build verification but no second user screenshot yet;
- the separate 192k/384 long-cadence `delta=3` runtime item remains unresolved.

Do not ask the user to repeat previously completed REAPER audible or product-validation tests merely for reassurance.

---

# Immediate next action

Freeze control-panel behavior unless a concrete runtime/UI issue is observed.

If no new panel issue appears, the next ASIO engineering task is the separate 192 kHz / 384-frame cadence closure, not additional UI refactoring.

Keep CTCDC/CTIntrfu and Creative application UI work out of this tab unless the user explicitly changes scope.

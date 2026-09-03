# CURRENT HANDOFF — Sound Blaster X4 Windows ARM64

Updated: 2026-09-04 KST

## Source of truth

Repository: `npark2860-cyber/SoundBlaster-X4-ARM64`

Default branch: `main`

Verified `main` before this handoff update:

`25d86f488593c9d3b435c1973da0d001b6a059a8`

Current ASIO SDK baseline source branch:

`exp/windows-arm64-asio-sdk-abi-baseline`

Verified branch HEAD before the latest runtime-only tests:

`a02be3c7ffb4dc66c7eb903712a8b4301efe8ea7`

At startup, verify actual GitHub branch heads again and read in this order:

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260904_ASIO_ACTIVE_PLAYBACK_COLLISION_RUNTIME.md`
3. `DEBUG_HISTORY_20260903_ASIO_WDF_CRASH_FINGERPRINT.md`
4. `DEBUG_HISTORY_20260903_ASIO_ENGINE_STAGE_A0_RUNTIME_SUCCESS.md`
5. `DEBUG_HISTORY_20260903_ASIO_WAVERT_ACTIVE_RUNTIME_SUCCESS.md`
6. `NEXT_ACTION_ASIO.md`
7. `DEBUG_HISTORY_20260903_WINDOWS_CTCDC_PATH.md`
8. `DEBUG_HISTORY_20260903_CTCDC_NATIVE_UNLOCK_TRACE.md`
9. `DEBUG_HISTORY_20260903_CTCDC_MAX_PAYLOAD_RUNTIME.md`
10. `DEBUG_HISTORY_20260903_CTCDC_OPEN_SESSION_RUNTIME.md`
11. `DEBUG_HISTORY_20260903_DIRECT_MODE_RUNTIME_SUCCESS.md`
12. `NEXT_ACTION_CTCDC.md`

For the SDK source implementation also read, from branch `exp/windows-arm64-asio-sdk-abi-baseline`:

- `DEBUG_HISTORY_20260903_ASIO_SDK_ABI_BASELINE_IMPLEMENTED.md`
- `README.md`
- `src/asio-sdk-abi-baseline/`

Do not reconstruct state from old chat context when GitHub is available.

## Project goal

Build independent native Windows ARM64 support for the locally USB-connected Creative Sound Blaster X4 / SB1815 on Snapdragon Windows ARM64.

The project now has two separate native components:

1. native ARM64 X4 controller over CTCDC/CDC for device controls such as Direct Mode;
2. native ARM64 ASIO user-mode implementation over Microsoft KS/WaveRT.

Creative binaries are reference material only. Final runtime architecture must not depend on or redistribute Creative x86/x64 binaries.

Device:

- Sound Blaster X4
- codename `Accent2`
- model/package `SB1815`
- USB VID/PID `041E:3278`

---

# ASIO — CURRENT PRIORITY

## Highest-value finding

The current ASIO blocker is no longer a generic ARM64 ABI problem.

The same official-Windows-SDK native ARM64 baseline executable:

`x4-asio-sdk-abi-baseline.exe`

SHA-256:

`8EB73A17D25BE4FCB005F1BCF4F7CEFAA830A8F5FD906C6E526DA2868626AAAC`

passes cleanly when no other X4 playback stream is active, but green-screens the machine when a second real Windows playback stream is deliberately kept active on the X4 during the baseline run.

Controlled runtime matrix:

| Audiosrv | CTAudSvcService | Other X4 playback active | Result |
|---|---|---|---|
| OFF | OFF | no | PASS |
| ON | OFF | no | PASS |
| ON | ON | no | PASS |
| ON | ON | yes | GREEN SCREEN |

Therefore:

- `Audiosrv` merely running is not sufficient to trigger the crash;
- `CTAudSvcService` merely running is not sufficient to trigger the crash;
- services ON/OFF are not the root differentiator;
- the tested differentiator is **another active X4 render stream**.

The active-playback reproducer must **not** be run again merely to reproduce the crash. The trigger has been demonstrated.

See `DEBUG_HISTORY_20260904_ASIO_ACTIVE_PLAYBACK_COLLISION_RUNTIME.md`.

## Known-good SDK baseline

With no competing active X4 playback stream, the SDK baseline hardware run is fully successful:

- native ARM64 (`0xAA64`)
- official Windows SDK structures
- compile-time ABI guards
- X4 `msft_wave` filter opens
- Render Pin 1 opens at 48 kHz / stereo / 16-bit
- `KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION` returns a 4096-byte cyclic buffer
- `CallMemoryBarrier = 0` on the hardware-confirmed runs
- notification event registration succeeds
- `KSSTATE_ACQUIRE` succeeds
- `KSSTATE_PAUSE` succeeds
- `KSSTATE_RUN` succeeds
- 20/20 notifications arrive
- packet count advances `1..20`
- presentation position advances monotonically
- packet discontinuities = 0
- position regressions = 0
- `RUN -> PAUSE -> ACQUIRE -> STOP` succeeds
- event unregister succeeds
- pin/filter close is clean

This is enough to exonerate a simple official-SDK ABI/layout failure and the basic single-stream WaveRT state sequence.

## Prior native Stage A0 result

The earlier freestanding native ARM64 Stage A0 also passes one equivalent lifecycle on hardware:

- 48 kHz / stereo / 16-bit
- Render Pin 1
- 4096-byte buffer
- NotificationCount 2
- zero once before RUN
- no RUN-time writes
- 20 notifications
- clean teardown

This remains valid evidence.

## Repeated crash fingerprint before coexistence isolation

Four prior native variants produced the same kernel fingerprint:

- Stage A
- Stage A1 repeated lifecycle
- Stage A1 delayed-reopen
- SDK ABI baseline

Common class:

- `WDF_VIOLATION 0x10D`
- Parameter 1 = `0x5`
- wrong WDF object type/lifetime path
- repeated `usbaudio2.sys` / WDF stack fingerprint

See `DEBUG_HISTORY_20260903_ASIO_WDF_CRASH_FINGERPRINT.md`.

## Kernel stale-pipe mechanism already proven in prior dump

For `090326-16234-01.dmp`, the crash path was:

`WdfRequestSend`
→ `usbaudio2!UAWdfUsbDataPipe::SendBufferToTarget`
→ `SubmitQueuedBuffers`
→ `UAWdfUsbIsoPipe::OnPipeRestart`
→ recovery work item

The failing handle:

`0x31F67D2BDAB8`

was a `WDFUSBPIPE` whose backing `FxUsbPipe` was already destroyed.

The X4 WDFDEVICE itself remained alive.

The victim `IsoStreamOut` contained:

- EP01 Data OUT wrapper at `+0xE8`
- EP83 Feedback IN wrapper at `+0x288`

The EP83 wrapper cached the exact stale crash handle.

The recovery worker does not refresh/recreate the cached pipe handle before resubmitting. This stale-handle recovery failure is proven for the prior dump.

## Prior crash victim was a different render stream

The prior crash-victim `IsoStreamOut` was decoded from live dump memory as:

- `WAVE_FORMAT_EXTENSIBLE`
- stereo
- 3 bytes/sample
- 24-bit
- channel mask `0x3`
- 96,000 Hz

It was therefore **not** the SDK baseline's requested 48 kHz / 16-bit render stream.

This is important because it matches the new controlled coexistence result: an already-existing render stream is the object seen failing in recovery while another KS/WaveRT render path is being created/used.

## Strong current model

Current evidence supports:

existing active X4 render stream
→ second KS/WaveRT render pin lifecycle begins
→ shared USB streaming interface / pipe lifecycle changes
→ existing EP01/EP83 WDFUSBPIPE objects become invalid
→ existing stream receives failed completions
→ `usbaudio2` recovery starts
→ recovery reuses a stale cached WDFUSBPIPE
→ `WDF_VIOLATION 0x10D / 5`

Precision rule:

- stale cached pipe recovery is proven in the prior dump;
- paired EP01/EP83 lifecycle destruction is strongly supported by WDF IFR;
- active concurrent X4 playback as the runtime trigger is hardware A/B confirmed;
- the exact `WdfUsbInterfaceSelectSetting` caller/timing that destroys the victim pipes is **not yet directly proven**.

Do not overclaim that final link.

## USB alternate-setting facts

Static `usbaudio2.sys` analysis established:

- `IsoStream::SetAltSettingActive` calls `WdfUsbInterfaceSelectSetting`
- `IsoStream::SetAltSettingInactive` selects setting index 0
- `IsoStream::AcquireIsoStream` calls `SetAltSettingActive`
- `IsoStreamOut::StopIsoStream` stops data pipes before selecting inactive setting
- `InitMaxTransportDelays` deliberately performs active/measurement/inactive setting selection during stream creation

WDF deletes previous pipe objects for an interface when selecting a new setting and creates new configured pipe objects for the selected setting. This is the strongest concrete mechanism candidate for the stale-pipe invalidation.

The direct `!wdfkd.wdfusbinterface` view from the triage dump was degraded and must not be used to assert an exact selected alt setting.

## Immediate ASIO next action

Do not run another intentional crash reproducer.

Next steps:

1. preserve the newest dump from the controlled active-playback green-screen test;
2. compare it against the prior `0x10D/5` / `usbaudio2!UAWdfUsbDataPipe::SendBufferToTarget` fingerprint;
3. if the fingerprint matches, close the ABI-root-cause investigation;
4. statically determine how the Creative x64 ASIO reference obtains render ownership/coexistence when Windows shared playback is active;
5. design the smallest safe native ARM64 preflight/ownership experiment;
6. only after coexistence is safe, resume ASIO COM Stage B/productization.

Do not move to capture, 24-bit ASIO transport, multichannel buffers, or unrelated ASIO features before the coexistence gate is understood.

See `NEXT_ACTION_ASIO.md`.

---

# CTCDC CONTROLLER — HARDWARE-CONFIRMED BASELINE

## Direct Mode works on Windows

The CTCDC protocol/transport discovery for Direct Mode is complete for the tested X4 state.

Hardware-confirmed path:

`COM3 open/configure`
→ CTCDC serial init
→ `5A 03 00` GetMaximumPayloadSize
→ RX `5A 03 02 3B 00`
→ Maximum Payload Size = 59
→ skip Unlock
→ skip `SW_MODE1`
→ `5A 09 01 02` GetFirmwareVersionString
→ firmware `1.7.250324.0910`
→ `5A 26 01 05` QueryButtonsAvailable
→ raw MIDAS write
→ `5A 39 03 00 05 01`
→ **physical X4 Direct Mode ON confirmed by the user**

Fixed Direct Mode frames:

- OFF: `5A 39 03 00 05 00`
- ON: `5A 39 03 00 05 01`

Construction:

- command `0x39` = FeatureControl
- SET operation = `0`
- Direct Mode bit position = `5`
- value = `0/1`
- header = `0x5A`

Do not revive obsolete `6A` or guessed `5C` variants.

## Windows USB topology

Composite device:

`USB\VID_041E&PID_3278`

Relevant interfaces:

- MI_00 / IF0: HID Consumer Control
- MI_01 / IF1+IF2: CDC ACM + CDC data, exposed as COM3 on the tested machine
- MI_03 / IF3: USB Audio 2.0 AudioControl
- IF4–IF6: USB AudioStreaming

CDC endpoints:

- Bulk OUT `0x03`
- Bulk IN `0x82`

Descriptor facts:

- total configuration length `1143`
- seven interfaces
- no vendor-class `0xFF` interface
- no UAC Extension Unit

## Native CTCDC fast path

Serial setup:

- event mask `0x05`
- 115200 baud
- 8N1
- DTR/RTS control bits disabled while preserving unrelated DCB flags
- zero COM timeouts
- `PurgeComm(0x0F)`
- `SETDTR`

Readiness query:

`5A 03 00`

Hardware RX:

`5A 03 02 3B 00`

Observed firmware:

`1.7.250324.0910`

Buttons query RX:

`5A 26 06 05 00 01 00 1E 00`

Do not assign undocumented meaning to the button payload.

## CTCDC fallback unlock

The fallback native unlock algorithm has been recovered but is not needed while the fast-path maximum-payload query succeeds.

Reference binaries remain reference-only and must not be committed or redistributed.

See the CTCDC debug-history documents for the exact AES-GCM fallback details.

## Eliminated controller paths

Do not repeat:

- Windows BLE control
- HID output/prefix guessing
- naked Direct Mode COM writes without CTCDC session setup
- UAC Extension Unit search
- vendor-class interface search
- `6A` Direct Mode variants
- guessed `5C` wrappers

## CTCDC next task

CTCDC protocol discovery is complete enough for narrow productization:

1. X4 CDC/COM discovery
2. CTCDC-compatible serial open/init
3. fast-path session validation
4. Direct Mode ON
5. Direct Mode OFF
6. clean close/release

Do not add unrelated Creative features in the first controller implementation step.

ASIO coexistence remains the project's current highest-priority blocker.

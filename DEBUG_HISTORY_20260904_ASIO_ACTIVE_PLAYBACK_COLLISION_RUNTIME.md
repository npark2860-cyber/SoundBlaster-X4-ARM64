# DEBUG HISTORY — ASIO active-playback collision isolated

Date: 2026-09-04 KST

## Purpose

Record the controlled runtime experiments that isolated the current native ARM64 ASIO blocker from generic SDK/ABI failure to a coexistence failure that appears only when another X4 playback stream is already active.

This document supersedes earlier working hypotheses that `Audiosrv` state, `CTAudSvcService` state, repeated reopen, per-notification DMA writes, or a simple hand-declared ARM64 ABI mismatch were sufficient to explain the `WDF_VIOLATION 0x10D / subtype 0x5` crashes.

## Fixed SDK baseline identity

All service/coexistence tests below used the same unmodified native ARM64 SDK baseline executable.

- executable: `x4-asio-sdk-abi-baseline.exe`
- SHA-256: `8EB73A17D25BE4FCB005F1BCF4F7CEFAA830A8F5FD906C6E526DA2868626AAAC`
- native machine: `0xAA64`
- source branch: `exp/windows-arm64-asio-sdk-abi-baseline`
- branch HEAD before these runtime-only tests: `a02be3c7ffb4dc66c7eb903712a8b4301efe8ea7`

The baseline uses official Windows SDK structures and the already-reviewed compile-time ARM64 ABI guards. No Creative runtime binary is loaded by this executable.

Baseline stream shape:

- `msft_wave`
- Render Pin 1
- 48 kHz
- stereo
- 16-bit PCM / `WAVE_FORMAT_EXTENSIBLE`
- 4096-byte WaveRT notification buffer
- NotificationCount = 2
- entire buffer zeroed once before RUN
- no writes during RUN
- exactly 20 notifications
- one `ACQUIRE -> PAUSE -> RUN -> PAUSE -> ACQUIRE -> STOP` lifecycle
- unregister notification event before close

## Controlled service-state matrix

### Test A — Windows Audio stopped

Condition:

- `Audiosrv = STOPPED`
- `CTAudSvcService = STOPPED` as a required dependent-service collateral stop
- no other X4-using application active

Result:

- PASS
- 20/20 notifications
- packet discontinuities = 0
- presentation-position regressions = 0
- clean unregister and close
- process exit code 0

### Test B — Creative Audio Service stopped only

Condition:

- `Audiosrv = RUNNING`
- `CTAudSvcService = STOPPED`
- no other X4-using application active

Result:

- PASS
- 20/20 notifications
- packet discontinuities = 0
- presentation-position regressions = 0
- clean unregister and close
- process exit code 0

### Test C — both services running, no active playback

Condition:

- `Audiosrv = RUNNING`
- `CTAudSvcService = RUNNING`
- Creative App, DAWs, browsers/media players, Discord and other likely X4 users closed
- no deliberately active playback stream

Result:

- PASS
- 20/20 notifications
- packet discontinuities = 0
- presentation-position regressions = 0
- clean unregister and close
- process exit code 0

### Consequence of A/B/C

Neither service being present is, by itself, sufficient to reproduce the kernel crash.

The earlier idea that `CTAudSvcService` alone was the collision trigger is withdrawn.

`Audiosrv` being active is also not sufficient.

## Test D — controlled active playback added

A fourth test changed one meaningful runtime condition relative to Test C: an actual Windows playback stream was held active on the X4 while the exact same SDK baseline was started.

Harness:

- both `Audiosrv` and `CTAudSvcService` remained RUNNING
- other user applications were closed
- X4 was the Windows default playback device
- the harness played a quiet continuous 440 Hz stereo WAV whose source file format was 96 kHz / 24-bit
- the baseline executable was unchanged
- the playback helper remained active while the baseline opened its 48 kHz / 16-bit Render Pin 1 stream

Important precision: the source WAV is 96 kHz / 24-bit, but a shared-mode Windows playback path may resample to the endpoint mix format. Therefore this test proves the presence of a concurrent active playback stream; it does not, by itself, prove the exact hardware stream format selected by the Windows audio engine.

Result:

- GREEN SCREEN / system bugcheck

The new dump has not yet been fingerprinted in WinDbg. Therefore the controlled A/B result proves the runtime trigger condition very strongly, but the latest crash is not yet claimed to be byte-for-byte the same stack fingerprint as the earlier dumps until the new dump is inspected.

## Controlled conclusion

The variable that flips the SDK baseline from PASS to system crash in the current matrix is not service state. It is the presence of another active X4 playback stream.

Observed matrix:

| Audiosrv | CTAudSvcService | Other X4 playback active | Result |
|---|---|---|---|
| OFF | OFF | no | PASS |
| ON | OFF | no | PASS |
| ON | ON | no | PASS |
| ON | ON | yes | GREEN SCREEN |

This makes concurrent active-render coexistence the primary blocker for the native ARM64 ASIO engine.

## Prior dump evidence that matches this result

The previously analyzed SDK-baseline dump `090326-16234-01.dmp` already established the following kernel-side failure mechanism.

Bugcheck:

- `WDF_VIOLATION (0x10D)`
- Parameter 1 = `0x5`
- wrong WDF framework object type
- process context = `System`
- fault path in `usbaudio2.sys`

Bucket/fault path:

`usbaudio2!UAWdfUsbDataPipe::SendBufferToTarget`

The exact crash handle in that dump was:

`0x31F67D2BDAB8`

WDF inspection showed that handle had been a `WDFUSBPIPE`, while the backing `FxUsbPipe` object was already in `FxObjectStateDestroyed`.

The owning WDFDEVICE was still alive. This was not whole-device removal.

## EP01 / EP83 stale-handle mechanism

The crash victim `IsoStreamOut` contained two `UAWdfUsbDataPipe` wrappers:

- Data OUT wrapper at `IsoStreamOut + 0xE8`
- Feedback IN wrapper at `IsoStreamOut + 0x288`

Endpoint mapping recovered from `IsoStreamOut::StartIsoStream` and live object bytes:

- `+0xE8` -> endpoint `0x01` -> EP01 Data OUT
- `+0x288` -> endpoint `0x83` -> EP83 Feedback IN

The EP83 wrapper cached the exact crash handle `0x31F67D2BDAB8`.

`UAWdfUsbIsoPipe::OnPipeRestart` does not reacquire a fresh configured WDFUSBPIPE. It changes recovery state and calls `SubmitQueuedBuffers`, which reaches `SendBufferToTarget` using the cached pipe handle.

Thus the direct bugcheck mechanism is:

1. the underlying WDFUSBPIPE is closed/destroyed,
2. the `usbaudio2` wrapper still retains the old handle,
3. recovery restarts,
4. recovery submits through the stale cached handle,
5. WDF detects the invalid object type/lifetime and bugchecks with `0x10D / 5`.

That stale-handle recovery mechanism is proven for the prior dump.

## WDF IFR chronology

The prior WDF crash log also showed EP01 and EP83 being canceled/closed/disposed as a pair.

Immediately before the final stale-handle failure, both streaming targets were reported closed with `STATUS_INVALID_DEVICE_STATE`, and disposal records for EP01 and EP83 shared the same dispose-event context.

This strongly indicates a common USB-interface/pipe lifecycle event rather than two unrelated endpoint failures.

`SetState(2)`, `CheckDeviceConnection`, and `RestartWorkItemRoutine` themselves do not recreate or select the USB interface setting. The recovery worker simply reuses the wrapper's cached handle.

## Prior victim was not the 48 kHz / 16-bit SDK probe stream

The crash-victim `IsoStreamOut` at:

`ffffce09`8fa8a010`

contained this 20-byte `WaveFormatInfo` block at `+0x38`:

`01 00 00 00 FE FF 02 00 03 00 18 00 03 00 00 00 00 77 01 00`

Recovered fields:

- format tag `0xFFFE` = `WAVE_FORMAT_EXTENSIBLE`
- channels = 2
- bytes/sample = 3
- valid bits = 24
- channel mask = `0x3` = stereo FL/FR
- sample rate = `0x00017700` = 96,000 Hz

Therefore that crash victim was a 96 kHz / stereo / 24-bit render stream, not the SDK baseline's requested 48 kHz / stereo / 16-bit stream.

This is the strongest prior-dump evidence that a pre-existing render stream was the object that later crashed in recovery while the test probe was creating/using another render path.

## USB alternate-setting lifecycle interpretation

Static analysis established:

- `IsoStream::SetAltSettingActive` calls `WdfUsbInterfaceSelectSetting`
- `IsoStream::SetAltSettingInactive` selects setting index 0
- `IsoStream::AcquireIsoStream` locks the sampling frequency and calls `SetAltSettingActive`
- `IsoStreamOut::StopIsoStream` stops both data pipes before `SetAltSettingInactive`
- `InitMaxTransportDelays` deliberately performs active -> measurement -> inactive selection during stream creation

Microsoft WDF behavior is that selecting a different USB interface setting deletes the previously configured WDFUSBPIPE objects for that interface and creates pipe objects for the newly selected setting.

This provides a concrete mechanism that can explain why one stream's cached EP01/EP83 handles become stale when another stream changes the shared USB streaming interface lifecycle.

However, the exact initiating `WdfUsbInterfaceSelectSetting` caller at the moment of the prior crash was not captured directly. Therefore the precise internal causal chain remains:

- stale cached pipe recovery: proven
- common EP01/EP83 lifecycle destruction: strongly supported
- concurrent active playback as the runtime trigger: hardware A/B confirmed
- exact alternate-setting call/timing that destroys the victim stream's pipes: still to be proven

Do not overstate the last item.

## WDFKD mini-dump limitation

Direct `!wdfkd.wdfusbinterface` inspection of Interface 4 returned degraded data:

- repeated `Alt Setting 0`
- zero endpoints
- null interface descriptors

An earlier verbose interface dump had briefly shown `Alt Setting 3`, but because the direct interface object data are incomplete in the triage dump, neither output is accepted as authoritative evidence for the selected setting at crash time.

Do not use those degraded setting displays to claim an exact selected alternate setting.

## Hypotheses now withdrawn or demoted

The following are no longer sufficient root-cause explanations:

- `Audiosrv` merely running
- `CTAudSvcService` merely running
- SDK ABI structure mismatch
- hand-declared structure layout alone
- repeated lifecycle alone
- RUN-time half-buffer writes alone
- failure to unregister the WaveRT event at the documented high-level point
- whole USB device removal

The Stage A0 and SDK baseline can both complete a clean single WaveRT lifecycle when no other X4 playback stream is active.

## Current engineering consequence

The ASIO project should no longer spend its next cycle trying to repair the official SDK structure definitions or the basic one-stream WaveRT state sequence.

The critical engineering problem is coexistence/ownership of the X4 render interface when Windows already has an active playback stream.

A safe native ARM64 ASIO implementation must not blindly open/start another conflicting KS/WaveRT render path and rely on `usbaudio2.sys` to recover from the interface collision.

## Immediate next actions

1. **Do not rerun the active-playback crash reproducer.** The trigger has already been reproduced and another intentional bugcheck is unnecessary.
2. Preserve and inspect the newest dump from Test D if available.
3. Compare its bugcheck, `usbaudio2` stack, failing WDF handle type, and EP01/EP83 recovery path against `090326-16234-01.dmp`.
4. If the fingerprint matches, close the ABI/lifecycle-root-cause investigation and move to coexistence design.
5. Statically inspect the Creative x64 ASIO reference for how it obtains render ownership when Windows shared playback is active: endpoint/session coordination, format ownership, pin selection, stop/suspend behavior, or other preflight synchronization.
6. Design the smallest safe ARM64 preflight/ownership experiment before resuming ASIO COM Stage B.

Do not move to capture, 24-bit ASIO transport, multichannel ASIO buffers, or ASIO COM productization until the concurrent-render ownership rule is understood.

## Architectural rule remains unchanged

Final product code remains independent native ARM64 code.

Creative binaries are reference material only and must not become runtime dependencies or redistributable payloads.

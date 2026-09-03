# NEXT ACTION — Native ARM64 ASIO engine

Updated: 2026-09-04 KST

## Current status

Native ARM64 ASIO feasibility is hardware-confirmed on the Sound Blaster X4.

Known-good one-stream baseline:

- X4 `msft_wave` KS filter opens from native ARM64 user mode
- Render Pin 1 opens at 48 kHz / stereo / 16-bit PCM
- `KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION` returns a 4096-byte cyclic buffer
- notification event registration succeeds
- `KSSTATE_ACQUIRE`, `PAUSE`, `RUN`, and `STOP` succeed
- 20/20 DMA notifications are observed
- packet count advances `1..20`
- presentation position advances monotonically
- packet discontinuities = 0
- presentation-position regressions = 0
- event unregister and handle close complete cleanly

The current official-SDK executable is native ARM64 and uses Microsoft SDK ABI declarations/guards directly.

Executable SHA-256:

`8EB73A17D25BE4FCB005F1BCF4F7CEFAA830A8F5FD906C6E526DA2868626AAAC`

Source branch:

`exp/windows-arm64-asio-sdk-abi-baseline`

Verified pre-runtime-test branch HEAD:

`a02be3c7ffb4dc66c7eb903712a8b4301efe8ea7`

## Blocker isolated — concurrent active X4 playback

The previous kernel crashes are no longer treated as a generic ARM64 SDK ABI failure.

Controlled runtime matrix using the exact same unmodified SDK baseline:

| Audiosrv | CTAudSvcService | Other X4 playback active | Result |
|---|---|---|---|
| OFF | OFF | no | PASS |
| ON | OFF | no | PASS |
| ON | ON | no | PASS |
| ON | ON | yes | GREEN SCREEN |

Therefore service state alone is not sufficient.

The hardware-confirmed differentiator in the current matrix is an **already-active X4 playback stream** while the native KS/WaveRT baseline opens/starts another render stream.

See:

`DEBUG_HISTORY_20260904_ASIO_ACTIVE_PLAYBACK_COLLISION_RUNTIME.md`

## Prior kernel mechanism

The previously analyzed SDK-baseline dump `090326-16234-01.dmp` showed:

- `WDF_VIOLATION 0x10D`
- Parameter 1 = `0x5`
- failure in `usbaudio2!UAWdfUsbDataPipe::SendBufferToTarget`
- failing object was a stale/destroyed `WDFUSBPIPE`
- the owning WDFDEVICE remained alive

The crash-victim `IsoStreamOut` was decoded as:

- 96 kHz
- stereo
- 24-bit
- `WAVE_FORMAT_EXTENSIBLE`

It was not the probe's 48 kHz / 16-bit render stream.

The victim had cached EP01 Data OUT and EP83 Feedback IN WDFUSBPIPE handles. WDF IFR showed the two targets being canceled/closed/disposed as a pair, after which recovery attempted to reuse the stale EP83 handle and bugchecked.

This makes a shared-interface render coexistence/lifetime collision the strongest current model.

## What is now ruled out as a sufficient explanation

Do not spend the next cycle re-testing these as primary causes:

- `Audiosrv` merely running
- `CTAudSvcService` merely running
- simple official-SDK ABI/layout mismatch
- hand-declared ARM64 structure layout alone
- repeated reopen alone
- per-notification DMA writes alone
- the basic one-stream 48k/16 WaveRT lifecycle

Those were useful earlier isolation steps but no longer match the controlled runtime matrix.

## Immediate next action

### 1. Do not intentionally reproduce the green-screen again

The active-playback trigger has already been demonstrated. Re-running it adds risk without adding meaningful evidence.

### 2. Preserve and analyze the newest controlled-test dump

If the latest green-screen dump is available, compare it directly against the established fingerprint:

- bugcheck `0x10D / p1=5`
- `usbaudio2!UAWdfUsbDataPipe::SendBufferToTarget`
- failing WDF object type/lifetime
- EP01/EP83 wrapper state
- recovery work-item path

If it matches, mark the generic ABI-root-cause investigation closed.

### 3. Recover Creative's coexistence/ownership strategy statically

Use `CtU2As64.DLL` as reference-only evidence.

Determine what the x64 Creative ASIO implementation does before it opens/starts its WaveRT render path when Windows shared playback may already be active.

Focus only on evidence for:

- endpoint/session ownership or preflight checks
- whether a Windows/Creative playback stream is stopped, suspended, drained, or otherwise coordinated
- pin/format selection strategy
- sampling-frequency ownership
- alternate-setting coordination
- state/event/thread synchronization around stream startup and teardown

Do not infer behavior merely from API names; require static call-path evidence.

### 4. Design one safe coexistence preflight experiment

The next ARM64 hardware experiment must avoid deliberately driving the known crash condition.

The goal is to detect or safely arbitrate an already-active X4 render stream **before** opening/starting the conflicting KS/WaveRT path.

Do not implement broad ASIO functionality in this step.

### 5. Resume ASIO COM Stage B only after coexistence is safe

After ownership/preflight is hardware-confirmed, continue toward:

native ARM64 DAW
→ native ARM64 ASIO COM DLL
→ SetupAPI / `KsCreatePin`
→ WaveRT
→ Microsoft UAC2
→ X4

No custom kernel driver is currently justified.

## Scope freeze until coexistence gate passes

Do not add or test yet:

- capture
- 24-bit ASIO transport
- multichannel ASIO buffers
- sample-rate expansion
- runtime buffer-writing/callback engine changes
- repeated reopen stress
- ASIO COM registration/productization beyond what is needed for the ownership experiment

## Architectural rule

Final product code remains independent native ARM64 code.

Creative binaries are reference material only. Do not load or redistribute `CtU2As64.dll`, `CTCDC.dll`, `CTIntrfu.dll`, or Creative application assemblies as final runtime dependencies.

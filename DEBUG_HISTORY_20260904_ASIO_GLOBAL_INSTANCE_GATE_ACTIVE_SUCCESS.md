# DEBUG HISTORY — ASIO GLOBAL INSTANCE GATE ACTIVE PLAYBACK SUCCESS

Date: 2026-09-04 KST

## Scope

Hardware validation of the native Windows ARM64 `GLOBALCINSTANCES` safety gate derived from static analysis of Creative `CtU2As64.DLL`.

The test executable is the SDK ABI baseline with one controlled change: before the real `KsCreatePin` call for X4 Render Pin 1, query `KSPROPERTY_PIN_GLOBALCINSTANCES` and fail closed when `CurrentCount >= PossibleCount`.

Experiment branch:

`exp/windows-arm64-asio-global-instance-gate`

Validated branch HEAD before this runtime result:

`362d58372b58640ac666dd59f17e532b092c05d3`

## Prior idle validation

With no active X4 playback:

- `PossibleCount = 1`
- `CurrentCount = 0`
- gate reported `busy=NO`
- real `KsCreatePin` executed
- complete 20-notification WaveRT lifecycle passed
- packet discontinuities = 0
- position regressions = 0
- clean teardown

Therefore the gate did not disturb the known-good idle baseline.

## Active playback validation

While normal Windows playback through the Sound Blaster X4 was actively running, the exact same executable produced:

```text
GLOBAL INSTANCE GATE: PinId=1 PossibleCount=1 CurrentCount=1 busy=YES
GLOBAL INSTANCE GATE: BUSY -> KsCreatePin SKIPPED
KsCreatePin failed status=0x000000AA
notifications=0
packet_discontinuities=0
position_regressions=0
SDK ABI BASELINE RESULT: FAIL
```

The process returned normally. No green screen occurred.

`0xAA` is Win32 `ERROR_BUSY` returned intentionally by the experiment wrapper. The final baseline `FAIL` is expected because no WaveRT lifecycle is started when the preflight gate rejects pin creation.

## Hardware-confirmed result

The previously crash-inducing coexistence condition is now intercepted safely before `KsCreatePin`:

Windows X4 playback ACTIVE
-> Render Pin 1 `GLOBALCINSTANCES = 1 / 1`
-> native ARM64 gate reports BUSY
-> `KsCreatePin` is not called
-> no WaveRT buffer/state lifecycle begins
-> process exits normally
-> no WDF crash

This is the first hardware-confirmed native ARM64 mitigation for the active-playback collision.

## Root-cause closure level

The following chain is now supported by both static Creative reference evidence and X4 hardware A/B validation:

1. X4 Render Pin 1 has global instance capacity 1.
2. Windows shared playback consumes that one global instance.
3. Creative `CtU2As64.DLL` checks pin instance availability before pin instantiation and does not blindly proceed while capacity remains exhausted.
4. The original native ARM64 SDK baseline lacked this gate and could call `KsCreatePin` while another X4 render stream was active.
5. That collision was associated with the repeated `usbaudio2.sys` stale-WDFUSBPIPE recovery fingerprint (`WDF_VIOLATION 0x10D`, parameter 1 = 5).
6. Adding the `GLOBALCINSTANCES` gate prevents entry into the known crash condition on hardware.

This is sufficient to close the generic ABI/layout hypothesis and to establish missing render ownership/preflight as the first concrete root-cause differential.

## What remains unproven

- Whether `GLOBALCINSTANCES` alone covers every possible Creative coexistence case.
- Whether the product should eventually implement Creative-style `TakeExclusiveControl` / WASAPI exclusive arbitration instead of returning BUSY.
- Capture-side ownership behavior.
- Multichannel and other format ownership rules.

These remain out of scope until the render safety gate is integrated into the native ARM64 ASIO engine path.

## Next action

Integrate the same fail-closed `KSPROPERTY_PIN_GLOBALCINSTANCES` gate immediately before every native ARM64 X4 render `KsCreatePin` operation in the ASIO engine path.

For the first integration stage:

- no WASAPI exclusive arbitration
- no capture
- no 24-bit transport expansion
- no multichannel expansion
- no sample-rate expansion
- no lifecycle changes beyond the preflight gate

Expected product behavior when another X4 render stream is active: return a clean device-busy/coexistence error and do not instantiate the KS render pin.

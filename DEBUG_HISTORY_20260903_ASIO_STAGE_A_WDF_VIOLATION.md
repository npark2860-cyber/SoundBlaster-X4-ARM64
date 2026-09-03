# DEBUG HISTORY — ASIO Stage A native runtime WDF_VIOLATION

Updated: 2026-09-03 KST

## Status

**Quarantined. Do not rerun the current Stage A native executable.**

The user reported that running the first native ARM64 Stage A ASIO engine caused Windows to reboot with a green-screen bug check identified as `WDF_VIOLATION`, remembered as `0x10D`.

Microsoft defines bug check `0x10D WDF_VIOLATION` as a Kernel-Mode Driver Framework (KMDF) detected error in a framework-based kernel driver.

This is materially different from an ordinary user-mode crash.

## Important contrast

The immediately preceding PowerShell/C# active WaveRT probe succeeded on the same physical X4 and Windows ARM64 system with:

- `KsCreatePin` on Render Pin 1
- `KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION`
- 4096-byte cyclic buffer
- notification event registration
- `KSSTATE_ACQUIRE`
- `KSSTATE_PAUSE`
- `KSSTATE_RUN`
- 20/20 DMA notifications
- `KSPROPERTY_RTAUDIO_PACKETCOUNT`
- `KSPROPERTY_RTAUDIO_PRESENTATION_POSITION`
- clean `PAUSE -> ACQUIRE -> STOP`
- notification event unregister
- clean pin close

Therefore this crash does **not** overturn the prior hardware feasibility result. It indicates a defect or unsafe behavioral difference in the first native Stage A implementation and/or an exposed driver bug triggered by that difference.

## Native Stage A differences that must be isolated

The first native implementation expanded several variables at once compared with the hardware-proven C# probe:

1. switched implementation language/ABI from managed P/Invoke to freestanding native C++ ARM64
2. changed from one stream lifecycle to three complete reopen/run/close cycles
3. changed from 20 notifications to 64 notifications per run
4. began writing the logical half-buffer on every notification instead of filling silence once before RUN
5. continuously queried packet count and presentation position during the longer run
6. exercised rapid unregister/close/reopen lifecycle repeatedly

This violated the project's variable-isolation discipline. Do not repeat that expansion.

## Current hypotheses — not yet proven

Priority order for investigation:

1. native ABI / structure layout / DeviceIoControl buffer-direction mismatch not caught by the static asserts
2. lifecycle race caused by rapid pin teardown and reopen
3. buffer-write protocol mismatch on notification-driven WaveRT render (for example a packet/write-commit requirement not exercised by the read-only C# probe)
4. kernel driver defect exposed by the longer/repeated native sequence

Do not claim a specific root cause until the partial Stage A log and/or crash dump identifies the failing phase.

## Immediate next action

Do not produce another active RUN executable yet.

First:

- inspect any surviving `x4-asio-engine-stage-a.txt`
- inspect Windows minidump if available
- identify bug-check parameters and faulting module with `!analyze -v`
- compare the native C++ request ABI and ordering against the exact successful C# active probe

Then rebuild a minimal native parity probe with **one variable changed at a time**, starting with a single run and the same 20-notification behavior as the successful C# probe.

## Architectural conclusion retained

The successful managed probe remains hardware evidence that the X4's Microsoft USB Audio 2.0 WaveRT path supports the primitives needed for an ARM64 ASIO engine.

The current native Stage A executable is unsafe and must not be reused.

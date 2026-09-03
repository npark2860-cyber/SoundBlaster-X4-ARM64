# DEBUG HISTORY — ASIO Stage A native runtime WDF_VIOLATION

Updated: 2026-09-03 KST

## Status

**Quarantined. Do not rerun the current Stage A native executable.**

The first native ARM64 Stage A ASIO engine caused a kernel bug check on first hardware execution. The uploaded minidump now confirms the exact failure and materially narrows the fault path.

## Crash dump identity

Uploaded archive:

`090326-18031-01.zip`

Contained dump:

`090326-18031-01.dmp`

Dump size:

`3,504,636 bytes`

SHA-256:

`d56f467d4b08cf498a8a7b360024c73079e0c9d950cfe73885c03da2ab62e429`

The dump is a Windows ARM64 kernel triage dump (`MachineImageType = 0xAA64`).

The active process path preserved in the dump contains:

`C:\SB\x4-asio-engine-stage-a.exe`

## Exact bug check

Bug check:

`0x10D WDF_VIOLATION`

Parameters:

- Arg1 = `0x5`
- Arg2 = `0x000019748551C768`
- Arg3 = `0x1200`
- Arg4 = `0xFFFFE68B5DD31890`

Microsoft defines `WDF_VIOLATION / Arg1 = 0x5` as:

> A framework object handle of the incorrect type was passed to a framework object method.

Arg2 is the handle value that was passed. Microsoft documents Arg3 as reserved for this bug-check subtype, so do not assign an undocumented semantic meaning to `0x1200` without additional symbol evidence.

This is not an ordinary user-mode crash. KMDF deliberately bug-checked because a framework-based kernel driver called a WDF method with a framework object handle of the wrong type.

## Reconstructed ARM64 crash stack

The triage dump contains the ARM64 CONTEXT, call-stack storage, 311 loaded-driver records, and module base/size information. Frame-pointer walking from the saved ARM64 context yields the following module chain.

Current PC:

- `ntoskrnl.exe + 0x25ACEC`

Return chain:

1. `ntoskrnl.exe + 0x25B6B4`
2. `Wdf01000.sys + 0x6ED2C`
3. `Wdf01000.sys + 0x1200C`
4. `usbaudio2.sys + 0x7FC4`
5. `usbaudio2.sys + 0x12374`
6. `usbaudio2.sys + 0x139C0`
7. `usbaudio2.sys + 0x12084`
8. `usbaudio2.sys + 0x3E044`
9. `Wdf01000.sys + 0x1B224`
10. `Wdf01000.sys + 0x1B3F0`
11. `ntoskrnl.exe + 0x24B054`
12. `ntoskrnl.exe + 0x376450`
13. `ntoskrnl.exe + 0x2F1174`
14. `ntoskrnl.exe + 0x62D654`

Relevant loaded module ranges from the dump:

- `Wdf01000.sys`: base `0xFFFFF80134E00000`, size `0xD7000`
- `ks.sys`: base `0xFFFFF8013C150000`, size `0x82000`
- `portcls.sys`: base `0xFFFFF8013C990000`, size `0x6E000`
- `usbaudio2.sys`: base `0xFFFFF8013D320000`, size `0x5F000`

## What the stack proves

The WDF validator is the component that detects and reports the invalid framework-object type; `Wdf01000.sys` itself should not be labeled the defective driver merely because it issues the bug check.

The framework-driver call path immediately below the WDF validation frames is **`usbaudio2.sys`**. Multiple consecutive `usbaudio2.sys` frames are preserved before control returns through WDF and the kernel.

Therefore the dump provides strong direct evidence for this narrower statement:

> The Stage A request sequence reached a Microsoft `usbaudio2.sys` KMDF path in which a WDF method received a framework object handle of the wrong type, and WDF terminated the system with `0x10D/5`.

This does **not** yet prove whether the root cause is solely an internal `usbaudio2.sys` defect or whether the native user-mode client supplied an unusual/invalid request sequence that exposed insufficient validation or a lifetime bug inside `usbaudio2.sys`.

## Important contrast — WaveRT feasibility remains valid

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

Therefore the crash does **not** overturn the prior hardware feasibility result. It isolates a failure in the widened native Stage A execution pattern and/or a driver bug triggered by that pattern.

## ABI checks completed after the crash

The critical handwritten native definitions were compared with the documented Windows KS/WaveRT layouts:

- `KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION`: expected 64-bit size 40 bytes — native Stage A used 40
- `KSRTAUDIO_BUFFER`: pointer + ULONG + BOOL, expected 64-bit size 16 bytes — native Stage A used 16
- notification event property: `KSPROPERTY + HANDLE`, expected 64-bit size 32 bytes — native Stage A used 32
- `KSAUDIO_PRESENTATION_POSITION`: two 64-bit fields, expected size 16 bytes — native Stage A used 16
- WaveRT property IDs 5/6/7/9/10 match `BUFFER_WITH_NOTIFICATION`, register event, unregister event, packet count, and presentation position

A simple obvious size mismatch in these particular structures is therefore less likely than initially suspected, but handwritten ABI remains unnecessary risk and must be removed from the next native test.

## Native Stage A differences that remain to isolate

Compared with the hardware-proven C# probe, the failed native Stage A simultaneously changed:

1. managed P/Invoke -> freestanding handwritten native ARM64 ABI
2. one stream lifecycle -> three complete reopen/run/close cycles
3. 20 notifications -> 64 notifications per run
4. silence initialized once before RUN -> half-buffer writes after every notification while RUNNING
5. a substantially longer sequence of packet/presentation queries
6. rapid unregister/close/reopen lifecycle

The dump proves the kernel failure is in a `usbaudio2.sys` -> WDF object path, but it does not identify which of these widened behaviors triggers the invalid internal object lifetime/type.

## Current root-cause ranking — not yet final

1. **usbaudio2 internal lifetime/state bug exposed by the widened stream lifecycle**, especially teardown/reopen or a longer uncommon direct-WaveRT sequence
2. **native semantic/request mismatch** that the driver does not safely reject and which leads to internal WDF object misuse
3. repeated 64-notification query sequence exposing a delayed driver state/lifetime bug
4. per-notification direct DMA-buffer writes; still remove for parity, although the write itself does not call a WDF API and therefore maps less directly to the observed wrong-handle bug check
5. gross ABI structure-size error in the checked WaveRT structs — now lower probability after layout verification

Do not select one as final root cause until a controlled parity run or surviving Stage A runtime log identifies the exact phase.

## Immediate next action

Do not proceed to ASIO COM Stage B and do not reuse the quarantined Stage A executable.

The next native test must be a strict parity reproduction of the successful C# active probe:

- use official Windows SDK/WDK headers, not handwritten KS/WaveRT ABI definitions
- one X4 `msft_wave` open only
- Render Pin 1 only
- 48 kHz / stereo / 16-bit PCM
- 4096-byte buffer, notification count 2
- fill the entire cyclic buffer with silence once before RUN
- no DMA-buffer writes while RUNNING
- 20 notifications only
- packet count + presentation position only as in the successful managed probe
- one `PAUSE -> ACQUIRE -> STOP`
- one unregister and close
- no reopen in the same process
- flush a log before and after every active KS/WaveRT call boundary

If that exact native parity test is safe, add only one variable per later run:

A. extend 20 -> 64 notifications, still one lifecycle and no buffer writes

B. add per-notification half-buffer writes, still one lifecycle

C. add a second lifecycle only after the single-lifecycle cases are stable

D. add a third lifecycle last

A surviving `x4-asio-engine-stage-a.txt` from the crashed run remains useful because it can reveal which phase was reached immediately before the bug check, but the minidump already establishes the `usbaudio2.sys`/WDF failure path.

## Architectural conclusion retained

The successful managed probe remains hardware evidence that the X4's Microsoft USB Audio 2.0 WaveRT path supports the primitives needed for an ARM64 ASIO engine.

The current native Stage A executable is unsafe and must not be reused.
# DEBUG HISTORY — ASIO Stage A1 repeated-lifecycle WDF violation

Date: 2026-09-03 KST

## Test under investigation

Executable: `x4-asio-engine-stage-a1-lifecycle.exe`

Stage A1 changed exactly one runtime variable from the hardware-confirmed Stage A0 success:

- Stage A0: one complete open -> RUN -> STOP -> close lifecycle
- Stage A1: repeat that same lifecycle three times in one process

Still fixed in A1:

- Render Pin 1
- 48 kHz
- stereo
- 16-bit PCM
- 4096-byte WaveRT cyclic buffer
- notification count 2
- 20 notifications per lifecycle
- entire WaveRT buffer zeroed once before RUN
- no DMA/half-buffer writes while RUN is active
- no callback abstraction
- no capture / 24-bit / 96/192 kHz / multichannel / ASIO COM

## Runtime result

The machine bugchecked/rebooted during Stage A1.

Uploaded dump:

`090326-16687-01.dmp`

The dump contains the active executable path:

`C:\SB\x4-asio-engine-stage-a1-lifecycle.exe`

and executable SHA-256:

`1aac2db272bf5ac65058300844a830d14f9bcd12a2443f123061aa36251ae30e`

## Dump-header facts

- Machine: ARM64 / `0xAA64`
- BugCheck: `0x10D` = `WDF_VIOLATION`
- Parameter 1: `0x5`
- Parameter 2: `0x4BFA0D321FD8`
- Parameter 3: `0x1200`
- Parameter 4: `0xFFFFB405B77A5920`

Microsoft documents `0x10D / Parameter 1 = 0x5` as:

> A framework object handle of the incorrect type was passed to a framework object method.

Parameter 2 is the handle value passed in. Parameter 3 is documented as reserved for this subtype, so do not assign semantic meaning to `0x1200` without debugger/WDF extension evidence.

## Comparison with previous crash

The earlier Stage A crash also produced:

- BugCheck `0x10D`
- Parameter 1 `0x5`
- Parameter 3 `0x1200`

The handle value in Parameter 2 differed, as expected for a different runtime instance.

This establishes a repeatable WDF object-type failure pattern rather than a random system crash.

## What is now exonerated

Because Stage A0 passed natively and Stage A1 reproduced the crash with no buffer writes during RUN, the following are no longer primary suspects:

- native ARM64 process architecture
- the basic KS/WaveRT ABI used by the probe
- X4 `msft_wave` filter discovery
- `KsCreatePin` for Render Pin 1
- 48 kHz stereo 16-bit pin format
- first `BUFFER_WITH_NOTIFICATION` allocation
- first event registration
- first ACQUIRE / PAUSE / RUN / STOP lifecycle
- observing packet count and presentation position
- DMA half-buffer writes during RUN as the cause of this A1 crash

## New primary suspect

The crash can be triggered by repeating the WaveRT pin/resource lifecycle in the same process.

Current highest-value hypotheses, not yet proven:

1. Reopen occurs before the previous WaveRT pin/event resources are fully torn down asynchronously inside the Microsoft USB Audio / PortCls / WDF stack.
2. A specific cleanup ordering or immediate pin recreation exposes a stale internal WDF object handle/type transition.
3. The first lifecycle is valid but a later create/register/state transition re-enters the driver before deferred cleanup completes.

Do not call any of these proven until the Stage A1 checkpoint log identifies the exact lifecycle/call or a WinDbg WDF analysis identifies the faulting driver/function.

## Next action

Do not return to RUN-time buffer writes yet.

Next diagnostic should isolate repeated lifecycle timing/ordering only. Prefer:

- two lifecycles only
- preserve A0 behavior exactly
- insert a clearly defined delay after complete unregister/close before second reopen
- compare immediate-reopen versus delayed-reopen as separate one-variable experiments
- retain per-call persistent checkpoints

If the Stage A1 text log is available, inspect it before another runtime test because it may show whether the crash occurred during lifecycle 2 or 3 and which call was last reached.

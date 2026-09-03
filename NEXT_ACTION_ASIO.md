# NEXT ACTION — Native ARM64 ASIO engine

Updated: 2026-09-03 KST

## Current status

ASIO feasibility itself is hardware-confirmed on the user's Windows ARM64 Sound Blaster X4:

- X4 `msft_wave` KS filter opens from user mode
- Render Pin 1 opens at 48 kHz / stereo / 16-bit PCM
- `KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION` returns a real 4096-byte cyclic buffer
- notification event registration succeeds
- `KSSTATE_ACQUIRE`, `PAUSE`, `RUN`, and `STOP` succeed
- 20/20 DMA notifications were observed
- packet count and presentation position advance correctly
- presentation position advances by 512 frames per notification after startup

See `DEBUG_HISTORY_20260903_ASIO_WAVERT_ACTIVE_RUNTIME_SUCCESS.md`.

## Critical new issue — native Stage A quarantined

The first independent native ARM64 Stage A executable caused Windows to reboot with a reported `WDF_VIOLATION` / `0x10D` bug check on first hardware execution.

**Do not rerun that executable.**

See:

`DEBUG_HISTORY_20260903_ASIO_STAGE_A_WDF_VIOLATION.md`

The prior managed PowerShell/C# active probe remains valid hardware evidence because it completed the same core WaveRT stream lifecycle successfully.

The native implementation changed too many variables at once:

1. managed P/Invoke -> freestanding native ARM64 ABI
2. one stream lifecycle -> three reopen/run/close cycles
3. 20 notifications -> 64 per run
4. silence initialized once -> half-buffer writes on every notification
5. repeated packet/presentation queries across the longer run
6. rapid unregister/close/reopen lifecycle

Do not continue to ASIO COM Stage B until this native crash is isolated.

## Immediate next action

Forensics first, no new active RUN binary yet:

1. inspect any surviving `x4-asio-engine-stage-a.txt`
2. inspect the Windows minidump if available
3. obtain bug-check parameters and faulting module with `!analyze -v`
4. compare native structure layouts and DeviceIoControl buffer semantics against the exact successful C# probe
5. rebuild a native parity probe that changes **one variable only**

The next hardware run must match the successful managed probe as closely as possible:

- one open/run/stop/close lifecycle
- 48 kHz / stereo / 16-bit / Render Pin 1
- 4096-byte buffer / notification count 2
- 20 notifications only
- fill silence once before RUN
- no per-notification writes initially
- no repeated reopen

Only after that exact native parity run succeeds should logical 0/1 callback buffer writes be introduced.

## Architectural rule

Final product code remains independent native ARM64 code.

Creative binaries are reference material only. Do not load or redistribute `CtU2As64.dll`, `CTCDC.dll`, `CTIntrfu.dll`, or Creative application assemblies as runtime dependencies.

# DEBUG HISTORY — ASIO Stage A1D delayed reopen isolation

Date: 2026-09-03 KST

## Basis

Stage A0 hardware runtime passed one complete native ARM64 WaveRT lifecycle.

Stage A1 repeated lifecycle testing produced a reproducible ARM64 `WDF_VIOLATION` (`0x10D`, parameter 1 `0x5`). The current main-branch record is `DEBUG_HISTORY_20260903_ASIO_STAGE_A1_LIFECYCLE_WDF_VIOLATION.md`.

The Stage A1 dump archive contained only the `.dmp`; no persistent A1 text checkpoint log was available inside that archive. Therefore the exact crashing call within lifecycle 2/3 is not yet known.

## A1D diagnostic variable

A1D tests the teardown/deferred-cleanup timing hypothesis without returning to RUN-time buffer writes.

Runtime sequence:

1. Execute one complete A0-equivalent lifecycle.
2. Complete `RUN -> PAUSE -> ACQUIRE -> STOP`.
3. Unregister the WaveRT notification event.
4. Close event, pin, and filter handles.
5. Wait exactly **5000 ms after the first full clean close**.
6. Reopen and execute exactly one second A0-equivalent lifecycle.
7. Stop after lifecycle 2. There is no third lifecycle.

Fixed parameters remain:

- Sound Blaster X4 `msft_wave`
- Render Pin 1
- 48 kHz
- stereo
- 16-bit PCM
- 4096-byte WaveRT cyclic buffer
- `NotificationCount = 2`
- 20 notifications per lifecycle
- no DMA/half-buffer writes while RUN is active
- `PACKETCOUNT` and `PRESENTATION_POSITION` observation only
- no capture / 24-bit / 96 or 192 kHz / multichannel / ASIO COM

The 5-second delay is implemented with an unsignaled Win32 event plus `WaitForSingleObject(..., 5000)`, so the executable retains the same three DLL dependency families as A1: `KERNEL32.dll`, `SETUPAPI.dll`, and `KSUSER.dll`.

## Artifact

Executable:

`x4-asio-engine-stage-a1-delayed-reopen.exe`

Architecture verified:

- PE32+
- ARM64 / `coff-arm64`
- console subsystem

Executable SHA-256:

`cc82e83fd02d63a05225405930f2e85dd32530077788c70abf84fa5857d7a41f`

Distribution ZIP:

`SoundBlaster-X4-ASIO-Engine-Stage-A1D-Delayed-Reopen-ARM64.zip`

ZIP SHA-256:

`c53ab02065378cd0b43ec284070cf8366ce53cf745a12f2a3df87629db67f1d6`

## PASS criteria

Expected summary:

```text
notifications=40
packet_discontinuities=0
position_regressions=0
STAGE A1D DELAYED REOPEN RESULT: PASS
```

## Safety / interpretation

If this exact A1D test causes another green-screen/reboot, do not repeat it. Preserve the text log if present and the new minidump.

A PASS would strongly support the hypothesis that immediate reopen intersects deferred teardown/cleanup in the Windows USB Audio / PortCls / WDF path.

A repeated `0x10D/0x5` failure after the 5000 ms boundary would show that a simple short delay is not sufficient; the next variable should be cleanup ordering or a narrower reopen-call isolation, not DMA buffer writes.

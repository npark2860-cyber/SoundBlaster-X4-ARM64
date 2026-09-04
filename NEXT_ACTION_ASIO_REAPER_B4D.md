# NEXT ACTION — Stage B4D first REAPER proof

Updated: 2026-09-04 KST

Current validated B4D source:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

B4D ARM64EC build, registration, and normal COM registry host probe have passed.

Runtime-proven:

```text
CoCreateInstance hr=0x00000000
driverName=Sound Blaster X4 ARM64
driverVersion=107
B4D HOST PROBE RESULT: PASS (REGISTRY COM LOAD + IASIO VTABLE)
```

The next and only immediate milestone is the first real REAPER ARM64EC load/playback proof.

Keep variables frozen:

- 48 kHz
- stereo output
- 16-bit PCM
- 512-frame ASIO buffer
- no input/capture
- no 24-bit
- no extra sample rates
- no variable buffer size
- no MMCSS/AVRT changes

Procedure:

1. Stop normal Windows playback through X4.
2. Open REAPER ARM64EC.
3. `Options -> Preferences -> Audio -> Device`.
4. Set `Audio system: ASIO`.
5. Select `Sound Blaster X4 ARM64 ASIO`.
6. Verify output channels 1/2 appear.
7. Play a project or test signal.
8. Confirm audible X4 output.
9. Stop and start once more.
10. Close REAPER and verify the X4 stream is released cleanly.

Do not bypass BUSY. If REAPER reports the device unavailable while Windows playback owns the X4, treat that as expected safe refusal until the Windows stream is released.

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4D_REGISTER_HOST_PROBE_SUCCESS.md`.

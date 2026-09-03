# X4 ASIO Engine Stage A

> **QUARANTINED — DO NOT RUN THE CURRENT STAGE A EXECUTABLE.**
>
> First hardware execution of this native ARM64 Stage A build caused a Windows `WDF_VIOLATION` bug check / reboot on 2026-09-03. The preceding PowerShell/C# WaveRT active probe completed successfully on the same X4. Treat this native implementation as unsafe until the exact ABI/lifecycle difference is isolated. See `DEBUG_HISTORY_20260903_ASIO_STAGE_A_WDF_VIOLATION.md` on `main`.

Independent native Windows ARM64 WaveRT render-engine prototype for Sound Blaster X4.

## Original fixed scope

- X4 `msft_wave` filter discovery
- Render Pin 1 only
- 48 kHz
- stereo
- 16-bit PCM
- 4096-byte WaveRT cyclic buffer
- notification count 2
- two logical 512-frame host buffers
- callback index from completed packet count (`0/1/0/1/...`)
- sample position from `KSPROPERTY_RTAUDIO_PRESENTATION_POSITION`
- packet continuity from `KSPROPERTY_RTAUDIO_PACKETCOUNT`
- clean notification unregister / STOP / close
- 3 complete open-run-stop-close cycles, 64 callbacks each

No capture, 24-bit, 96/192 kHz, multichannel, dynamic buffer size, sample-rate switching, ASIO COM registration, or Creative runtime DLLs are included in Stage A.

## Why this build is quarantined

The implementation changed too many variables at once relative to the previously hardware-proven C# active probe: native ABI, three reopen cycles, longer runs, per-notification buffer writes, and repeated teardown/reopen.

The next native build must return to exact parity with the successful C# probe and change one variable at a time.

Do not use the original Stage A runtime ZIP again.

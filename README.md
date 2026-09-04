# SoundBlaster-X4-ARM64

Independent Windows-on-Arm support for the **Creative Sound Blaster X4 / SB1815**.

The project now has two separate tracks:

1. **ASIO** — independent user-mode ASIO implementation over Microsoft KS/WaveRT / `usbaudio2.sys`.
2. **CTCDC control** — independent X4 device-control path over the USB CDC/COM interface.

Creative binaries are reference-only. The independent runtime must not load or redistribute Creative binaries.

## Current status

### ASIO — real REAPER playback proven

Stage B4D is hardware/user proven in the Windows-on-Arm REAPER build:

```text
REAPER ARM64EC
-> registered independent ARM64EC ASIO COM DLL
-> ASIO 2.x host contract / time-info callbacks
-> mapped WaveRT DMA
-> Microsoft usbaudio2.sys
-> Sound Blaster X4
-> audible project playback
```

Validated B4D source:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Proven first-use configuration:

- 48 kHz
- stereo output
- signed 16-bit PCM transport
- 512-frame ASIO buffer
- X4 `msft_wave`, Render Pin 1
- 4096-byte WaveRT cyclic buffer
- NotificationCount=2
- ASIO 2.x time-info callbacks
- normal COM/ASIO registration
- actual audible REAPER playback

The older Classic ARM64 B4C implementation is also hardware-proven and remains important if a future host becomes pure Classic ARM64 rather than ARM64EC.

### Creative reference driver

The existing `Creative SB USB RT ASIO` driver also works in the current REAPER ARM64EC environment.

That makes it a useful behavioral reference for completing the independent driver: supported sample rates, bit depth, buffer sizes, input/output channels, latency reporting, and lifecycle behavior can be compared on the same X4 and same host.

The goal is behavioral compatibility where useful, **not** copying proprietary implementation code.

### CTCDC / Direct Mode

The native Windows CTCDC path is separately hardware-proven for X4 Direct Mode.

Confirmed Direct Mode frames:

- OFF: `5A 39 03 00 05 00`
- ON: `5A 39 03 00 05 01`

The current project priority is to finish ASIO productization first, then return to broader CTCDC controls.

## ASIO safety rule

A prior ungated second render stream could trigger a kernel `WDF_VIOLATION 0x10D/5` in `usbaudio2.sys` through a stale `WDFUSBPIPE` recovery path.

The current driver includes the Creative-equivalent pin-instance coexistence gate and safe BUSY refusal.

**Never bypass BUSY and do not intentionally reproduce the old green-screen collision.**

## Next ASIO milestone

Finish the independent driver as a practical first release rather than returning to tiny A/B/C/D micro-stages.

Priority order:

1. stability/reopen stress
2. 24-bit output
3. additional X4-supported sample rates
4. selectable ASIO buffer sizes
5. capture/input
6. multichannel output after the core release path is stable

The next development pass should first probe the working Creative reference driver and the X4 KS data ranges, then implement the supported capability set independently.

See:

- `CURRENT_HANDOFF.md`
- `NEXT_ACTION_ASIO.md`
- `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4D_REAPER_PLAYBACK_RUNTIME_SUCCESS.md`
- `DEBUG_HISTORY_20260904_CREATIVE_SB_USB_RT_ASIO_ARM64EC_RUNTIME.md`
- `NEXT_ACTION_CTCDC.md`

## Device

- Product: Sound Blaster X4
- Codename: `Accent2`
- Model/package: `SB1815`
- USB VID/PID: `041E:3278`

## Rule

Keep hardware-proven facts, static-analysis conclusions, and hypotheses explicitly separated. Do not treat an inferred protocol or driver behavior as confirmed until it is backed by runtime evidence, static evidence, or independent reproduction.

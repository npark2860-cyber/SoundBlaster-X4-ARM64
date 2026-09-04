# DEBUG HISTORY — ASIO COM Stage B4D REAPER playback runtime success

Updated: 2026-09-04 KST

## Result

Stage B4D first real DAW playback milestone is hardware/user confirmed PASS.

The Windows-on-Arm REAPER build successfully enumerated, selected, opened, and used the independent Sound Blaster X4 ARM64EC ASIO driver.

## Validated implementation

B4D source branch:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Validated B4D build workflow:

- run `33822642892`
- job `100868446837`
- result `success`
- checkout `a95a95d014bcc1c3a521be41325841ae96dc8a61`

Build-time ARM64EC/ARM64X verification passed for:

- `x4-asio-arm64ec.dll`
- `x4-asio-stage-b4d-smoke.exe`
- `x4-asio-stage-b4d-register.exe`
- `x4-asio-stage-b4d-host-probe.exe`

## Pre-REAPER registered-host proof

Observed:

```text
Registration verified. Probing normal COM registry load from ARM64EC host...
Sound Blaster X4 ASIO Stage B4D ARM64EC registered-host probe
CoInitializeEx hr=0x00000000
CoCreateInstance hr=0x00000000
driverName=Sound Blaster X4 ARM64
driverVersion=107
B4D HOST PROBE RESULT: PASS (REGISTRY COM LOAD + IASIO VTABLE)
```

This proved that a normal ARM64EC host could instantiate the registered ASIO COM class in-process and call the IASIO vtable before opening REAPER.

## REAPER enumeration / channel proof

REAPER successfully listed and selected:

`Sound Blaster X4 native ARM64 ASIO`

REAPER displayed the driver-provided output channels:

- `1: X4 Output L`
- `2: X4 Output R`

No input channels were exposed, matching the intentionally output-only first milestone.

## REAPER engine proof

REAPER displayed an active ASIO engine state at:

```text
48kHz
512spls
~10/10ms ASIO
```

The REAPER source/project status also displayed `24bit WAV`; this does not change the frozen driver transport, which remains 16-bit stereo PCM in B4D.

## Audible playback proof

The user then performed actual REAPER playback and explicitly confirmed that audio output through the Sound Blaster X4 worked correctly.

Therefore the following end-to-end path is now hardware/user proven:

```text
REAPER Windows ARM build (ARM64EC)
-> registered independent ARM64EC ASIO COM DLL
-> IASIO host contract / ASIO2 time-info callbacks
-> driver-owned host buffers
-> mapped WaveRT DMA
-> Microsoft usbaudio2.sys
-> Sound Blaster X4
-> audible real REAPER project playback
```

## What this proves

The project has passed beyond registry-free smoke testing. The independent driver is now proven as a real REAPER ASIO playback device on Windows ARM.

Proven first-use configuration:

- Windows on ARM / ARM64EC host
- Sound Blaster X4
- REAPER Windows ARM build
- 48 kHz
- stereo output
- 512-frame ASIO buffer
- ASIO 2.x time-info path
- actual audible DAW playback

## Still not implied

This first REAPER success does not yet prove:

- capture/input
- native 24-bit transport
- multichannel output
- sample rates other than 48 kHz
- variable ASIO buffer sizes
- long-duration stress stability
- repeated reopen/close stress
- MMCSS/AVRT tuning changes
- time code
- direct monitoring

The existing coexistence/BUSY safety rule remains mandatory. Never bypass BUSY and never intentionally reproduce the known old ungated collision.

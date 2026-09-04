# NEXT ACTION — Native ARM64 / ARM64EC ASIO

Updated: 2026-09-04 KST

## Hardware/runtime-confirmed layers

1. native ARM64 KS/WaveRT one-stream baseline
2. active-playback collision mechanism
3. Creative-equivalent pin-instance coexistence gate
4. native ARM64 ASIO COM Stage B0 ABI shell
5. Stage B1 coexistence preflight
6. Stage B2 fixed WaveRT lifecycle
7. Stage B3A ASIO double-buffer / `bufferSwitch` ABI
8. Stage B3B host PCM -> mapped WaveRT DMA with audible X4 output
9. Stage B4A asynchronous worker / joined stop lifetime
10. Stage B4B host query contract: channels, clock, block-aligned sample position/timestamp
11. Stage B4C ASIO 2.x time-info negotiation / `bufferSwitchTimeInfo`
12. corrected B4C FREE and BUSY paths — hardware PASS
13. Stage B4D ARM64EC / ARM64X build — Actions PASS
14. Stage B4D ASIO/COM registration + ARM64EC `CoCreateInstance()` host probe — PASS
15. REAPER Windows ARM enumeration/channel query — PASS
16. real REAPER project playback through Sound Blaster X4 — AUDIBLE PASS

Do not intentionally reproduce the known green-screen collision. Never bypass BUSY.

## Validated Stage B4C

Validated source:

`exp/windows-arm64-asio-com-stage-b4c-time-info@e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

Corrected FREE-path proof:

```text
init=1
initMessage=B4C init FREE: C 0/1 G 0/1; ASIO2 time-info available
B4C host asioMessage kAsioSupportsTimeInfo -> 1
timeInfoNegotiationCalls=1
...
callbackStats timeInfo=20 quiescentAfterStop=20 legacy=0 indexErrors=0 directProcessErrors=0 threadErrors=0 timeInfoErrors=0 positionErrors=0 timestampErrors=0 consistencyErrors=0 timestampAdvanced=YES hostSampleWrites=20480 lastPosition=9728
STAGE B4C TIME INFO RESULT: PASS (ASIO2 TIME-INFO CALLBACK + B4B TRANSPORT)
```

BUSY safety was separately hardware-proven:

```text
initMessage=B4C init BUSY: C 0/1 G 1/1; KsCreatePin SKIPPED
STAGE B4C TIME INFO RESULT: PASS (BUSY SAFELY BLOCKED AT INIT)
```

## Validated Stage B4D — ARM64EC + REAPER

Source branch:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Validated parent:

`e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

B4D keeps the hardware-proven B4C transport and adds ARM64EC adapter translation units plus registration/test tooling. The B4C implementation files themselves remain unchanged.

### Build proof

GitHub Actions:

- workflow: `Build ASIO COM Stage B4D REAPER ARM64EC`
- run `33822642892`
- job `100868446837`
- checkout `a95a95d014bcc1c3a521be41325841ae96dc8a61`
- result `success`

ARM64EC/ARM64X validation passed for:

- `x4-asio-arm64ec.dll`
- `x4-asio-stage-b4d-smoke.exe`
- `x4-asio-stage-b4d-register.exe`
- `x4-asio-stage-b4d-host-probe.exe`

### Registered-host proof

Observed:

```text
CoInitializeEx hr=0x00000000
CoCreateInstance hr=0x00000000
driverName=Sound Blaster X4 ARM64
driverVersion=107
B4D HOST PROBE RESULT: PASS (REGISTRY COM LOAD + IASIO VTABLE)
```

This proves the registered ARM64EC ASIO DLL loads through the normal COM path before REAPER.

### REAPER enumeration/channel proof

REAPER successfully listed and selected:

`Sound Blaster X4 native ARM64 ASIO`

Visible output channels:

- `1: X4 Output L`
- `2: X4 Output R`

No input channels are exposed yet, matching the intentionally output-only milestone.

### REAPER engine proof

REAPER displayed:

```text
48kHz
512spls
~10/10ms ASIO
```

The `24bit WAV` text visible in REAPER is source/project format information; the current driver transport remains fixed signed 16-bit stereo PCM.

### Real playback proof

The user performed actual REAPER playback and explicitly confirmed that playback through the Sound Blaster X4 works correctly.

Therefore the following path is now hardware/user proven end-to-end:

```text
REAPER Windows ARM build (ARM64EC)
-> registered independent ARM64EC ASIO COM DLL
-> ASIO 2.x host contract / time-info callbacks
-> driver-owned host buffers
-> mapped WaveRT DMA
-> Microsoft usbaudio2.sys
-> Sound Blaster X4
-> audible REAPER project playback
```

See:

- `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4D_REGISTER_HOST_PROBE_RUNTIME_SUCCESS.md`
- `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4D_REAPER_LOAD_RUNTIME_SUCCESS.md`
- `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4D_REAPER_ENGINE_RUNTIME_SUCCESS.md`
- `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4D_REAPER_PLAYBACK_RUNTIME_SUCCESS.md`

## Proven first-use configuration

Keep this as the validated baseline:

- Windows on ARM / ARM64EC host
- REAPER Windows ARM build
- Sound Blaster X4
- 48 kHz
- stereo output only
- signed 16-bit PCM transport
- ASIO buffer 512 frames
- X4 `msft_wave`, Render Pin 1
- WaveRT cyclic buffer 4096 bytes
- NotificationCount=2
- PacketCount-derived write-ahead slot
- both coexistence gates
- B4A worker lifetime
- B4C ASIO2 time-info behavior

## Immediate next work

The first real DAW playback milestone is complete. Do not reopen the core transport without a specific reason.

Next development should expand capability in controlled groups rather than returning to A/B/C/D micro-stages. Recommended order:

1. repeated REAPER start/stop/reopen/close stress and longer playback stability
2. 24-bit output transport
3. additional fixed sample rates
4. selectable ASIO buffer sizes
5. capture/input
6. multichannel output
7. MMCSS/AVRT tuning only if measurements show a need

Do not mix several capability expansions into one unmeasurable change.

## Still not proven

- capture/input
- native 24-bit transport
- multichannel output
- sample rates other than 48 kHz
- variable ASIO buffer sizes
- long-duration stress stability
- repeated reopen/close stress
- time code
- direct monitoring
- Creative runtime dependencies
- custom kernel driver

Creative binaries remain reference-only and must not be loaded or redistributed as runtime dependencies.

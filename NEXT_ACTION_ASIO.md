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

Do not intentionally reproduce the known green-screen collision.

## Validated Stage B4C

Validated source:

`exp/windows-arm64-asio-com-stage-b4c-time-info@e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

Corrected FREE-path proof:

```text
init=1
initMessage=B4C init FREE: C 0/1 G 0/1; ASIO2 time-info available
future(kAsioCanTimeInfo)=1061701536 expected=1061701536
B4C host asioMessage kAsioSupportsTimeInfo -> 1
timeInfoNegotiationCalls=1
startMessage=B4C start OK ... timeInfo=YES position=0
...
callbacksBeforeStop=20 legacyCallbacks=0
stopMessage=B4C stop OK workerJoined=YES notif=20 cb=20 dmaWrites=20 dmaFrames=10240
callbackStats timeInfo=20 quiescentAfterStop=20 legacy=0 indexErrors=0 directProcessErrors=0 threadErrors=0 timeInfoErrors=0 positionErrors=0 timestampErrors=0 consistencyErrors=0 timestampAdvanced=YES hostSampleWrites=20480 lastPosition=9728
getSamplePosition after stop=-996 expected=-996
DllCanUnloadNow hr=0x00000000
STAGE B4C TIME INFO RESULT: PASS (ASIO2 TIME-INFO CALLBACK + B4B TRANSPORT)
```

BUSY safety was also separately hardware-proven:

```text
initMessage=B4C init BUSY: C 0/1 G 1/1; KsCreatePin SKIPPED
STAGE B4C TIME INFO RESULT: PASS (BUSY SAFELY BLOCKED AT INIT)
```

See:

- `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4C_TIME_INFO_FIRST_RUNTIME.md`
- `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4C_CORRECTED_SMOKE_BUSY_RUNTIME.md`
- `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4C_TIME_INFO_RUNTIME_SUCCESS.md`

## REAPER architecture constraint

The Windows-on-Arm REAPER build is ARM64EC. ARM64EC/x64-compatible processes cannot load a Classic ARM64 in-process DLL.

Therefore the validated Classic ARM64 B4C DLL cannot be registered directly for REAPER. The same proven ASIO/WaveRT implementation must first be linked as ARM64EC/ARM64X.

This is a host ABI packaging change, not a change to the proven X4 transport algorithm.

## Stage B4D — combined ARM64EC + registration + REAPER milestone

Branch:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration`

Current implementation HEAD:

`a95a95d014bcc1c3a521be41325841ae96dc8a61`

Validated parent:

`e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

B4D deliberately combines what would otherwise have been multiple A/B/C/D substages:

1. ARM64EC host-ABI build
2. registry-free hardware smoke
3. elevated ASIO/COM registration + registry verification
4. normal `CoCreateInstance()` registered-host probe
5. first real REAPER load/playback test

The user should not need to stop after each substage for a new source branch.

### Source isolation

B4D does not modify these hardware-proven implementation files:

- `driver_b4c.cpp`
- `preflight.cpp`
- `wavert_engine_b4a.cpp`
- `smoke_b4c_monotonic.cpp`

Instead it adds thin ARM64EC adapter translation units that parse Windows/SDK/project headers under the real ARM64EC architecture first, re-assert the ASIO public structure ABI, and bypass only the inherited source files' development-only Classic-ARM64 `#error` guard.

Added:

- `driver_b4d_arm64ec.cpp`
- `preflight_b4d_arm64ec.cpp`
- `wavert_engine_b4d_arm64ec.cpp`
- `smoke_b4d_arm64ec.cpp`
- `register_b4d_arm64ec.cpp`
- `host_probe_b4d_arm64ec.cpp`
- `test_b4d.cmd`
- `install_b4d.cmd`
- `uninstall_b4d.cmd`
- `README_B4D.md`

B4C -> B4D compare contains only B4D additions plus `CMakeLists.txt`; the validated B4C/WaveRT source files themselves are not modified.

### Build target

CMake platform:

`ARM64EC`

The B4D build must reject Classic ARM64 and x64 configurations.

Final PE validation must require both:

- final machine `0x8664`
- Microsoft linker header dump contains `ARM64X`

Raw `0x8664` alone is not accepted because ordinary x64 uses the same final machine value.

### Manual workflow

`Build ASIO COM Stage B4D REAPER ARM64EC`

File:

`.github/workflows/build-asio-com-stage-b4d-reaper-arm64ec.yml`

Trigger remains `workflow_dispatch` only.

The workflow builds and validates:

- `x4-asio-arm64ec.dll`
- `x4-asio-stage-b4d-smoke.exe`
- `x4-asio-stage-b4d-register.exe`
- `x4-asio-stage-b4d-host-probe.exe`

The distribution also includes a byte-identical `x4-asio-arm64.dll` alias because the inherited registry-free B4C smoke loader intentionally keeps its validated DLL filename.

### Package test sequence

1. Close normal Windows playback using X4.
2. Run `test_b4d.cmd`.
3. Run `install_b4d.cmd`.
4. The install script requests elevation, registers the ASIO/COM DLL, verifies registry values, then automatically runs the normal COM host probe.
5. Only after both registration and host probe PASS, open REAPER.
6. In REAPER select `Audio system: ASIO` and `Sound Blaster X4 ARM64 ASIO`.
7. Keep the first real DAW proof frozen at 48 kHz / stereo / 512 frames.
8. Verify actual project/test-signal playback, stop/start again, then close REAPER cleanly.
9. `uninstall_b4d.cmd` removes the test registration if needed.

Expected pre-REAPER proof includes:

```text
B4D REGISTER RESULT: PASS
CoCreateInstance hr=0x00000000
driverName=Sound Blaster X4 ARM64
driverVersion=107
B4D HOST PROBE RESULT: PASS (REGISTRY COM LOAD + IASIO VTABLE)
```

## Frozen first REAPER configuration

Keep unchanged:

- 48 kHz
- stereo output only
- signed 16-bit PCM
- ASIO buffer 512 frames
- X4 `msft_wave` Render Pin 1
- WaveRT cyclic buffer 4096 bytes
- NotificationCount=2
- PacketCount-derived write-ahead slot
- both coexistence gates
- B4A worker lifetime
- B4C ASIO2 time-info behavior

Never bypass BUSY.

## Still excluded until first REAPER playback succeeds

- capture/input
- 24-bit transport
- multichannel
- extra sample rates
- variable buffer sizes
- MMCSS/AVRT tuning
- repeated reopen stress expansion
- time code
- direct monitoring
- Creative runtime dependencies
- custom kernel driver

## Architecture

Windows ARM REAPER (ARM64EC)
-> independent ARM64EC ASIO COM DLL
-> inherited B4C ASIO/WaveRT implementation
-> SetupAPI / `KsCreatePin` / mapped WaveRT cyclic buffer
-> Microsoft `usbaudio2.sys`
-> Sound Blaster X4

Creative binaries remain reference-only and must not be loaded or redistributed as runtime dependencies.

# Sound Blaster X4 ARM64 ASIO — Stage B4D ARM64EC + REAPER

B4D is the combined host-compatibility / registration milestone. It intentionally does not split ARM64EC porting, smoke, registration, registered-host loading, and first REAPER playback into separate A/B/C/D branches.

## Validated parent

B4D starts from the corrected, hardware-PASS B4C source:

`exp/windows-arm64-asio-com-stage-b4c-time-info@e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

B4C hardware proof includes:

- ASIO2 time-info negotiation
- `bufferSwitchTimeInfo()` only after negotiation
- logical sample positions `0, 512, 1024, ...`
- non-regressing `timeGetTime()` system time with observed advancement
- host time-info / `getSamplePosition()` consistency
- mapped WaveRT DMA copy
- asynchronous worker and joined stop
- clean dispose / unload

## Why ARM64EC

The Windows-on-Arm REAPER build is ARM64EC. An ARM64EC/x64-compatible process cannot load a Classic ARM64 in-process DLL, so the ASIO DLL used by REAPER must be ARM64EC-compatible.

B4D therefore recompiles the proven B4C implementation as ARM64EC while preserving the audio behavior.

## Source isolation

The validated B4C implementation files remain unchanged.

B4D adds thin ARM64EC adapter translation units:

- `driver_b4d_arm64ec.cpp`
- `preflight_b4d_arm64ec.cpp`
- `wavert_engine_b4d_arm64ec.cpp`
- `smoke_b4d_arm64ec.cpp`

Windows / SDK / project headers are parsed first under the real ARM64EC architecture macros. The adapters then bypass only the inherited source files' development-time Classic-ARM64 `#error` guards. Public ASIO structure sizes are re-asserted under ARM64EC before the adapter includes the inherited source.

The WaveRT / packet / callback algorithm is not rewritten.

## Frozen first REAPER configuration

Do not expand variables during the first REAPER proof:

- 48 kHz
- stereo output only
- signed 16-bit PCM
- ASIO 512 frames
- X4 `msft_wave` Render Pin 1
- WaveRT 4096 bytes / NotificationCount=2
- existing init and pre-pin coexistence gates
- no SETWRITEPACKET
- no capture
- no 24-bit
- no extra sample rates
- no variable buffer size
- no MMCSS/AVRT changes

## Package files

- `x4-asio-arm64ec.dll` — actual ARM64EC REAPER DLL
- `x4-asio-arm64.dll` — byte-identical compatibility alias used by the inherited registry-free smoke loader
- `x4-asio-stage-b4d-smoke.exe` — ARM64EC registry-free B4C-equivalent hardware smoke
- `x4-asio-stage-b4d-register.exe` — ARM64EC register/unregister/verify helper
- `x4-asio-stage-b4d-host-probe.exe` — ARM64EC normal COM/registry in-process load probe
- `test_b4d.cmd`
- `install_b4d.cmd`
- `uninstall_b4d.cmd`

## Test sequence

### 1. ARM64EC smoke

Close applications that are playing through the X4, then run:

```bat
test_b4d.cmd
```

A BUSY result is a safe refusal. Never bypass it.

The FREE path should retain the corrected B4C final proof:

```text
callbackStats ... legacy=0 ... timeInfoErrors=0 positionErrors=0 timestampErrors=0 consistencyErrors=0 timestampAdvanced=YES ...
STAGE B4C TIME INFO RESULT: PASS (ASIO2 TIME-INFO CALLBACK + B4B TRANSPORT)
```

The inherited final label still says B4C because B4D deliberately reuses the validated smoke implementation rather than rewriting it.

### 2. Register + normal host load probe

Run:

```bat
install_b4d.cmd
```

The script requests elevation and the registration helper verifies:

- HKLM COM CLSID
- `InprocServer32` points to the packaged `x4-asio-arm64ec.dll`
- `ThreadingModel=Apartment`
- HKLM `SOFTWARE\\ASIO\\Sound Blaster X4 ARM64 ASIO`
- ASIO CLSID matches the COM CLSID

Expected registration result:

```text
B4D REGISTER RESULT: PASS
```

After registration, the script automatically runs `x4-asio-stage-b4d-host-probe.exe` without direct `LoadLibrary` or manual DLL path selection. The probe uses normal `CoCreateInstance()` discovery through the registry and then calls the IASIO vtable only for metadata.

Expected host proof:

```text
CoCreateInstance hr=0x00000000
driverName=Sound Blaster X4 ARM64
driverVersion=107
B4D HOST PROBE RESULT: PASS (REGISTRY COM LOAD + IASIO VTABLE)
```

This proves the ARM64EC host can discover and load the registered in-process ASIO DLL before REAPER is involved.

### 3. First REAPER proof

Start the Windows ARM build of REAPER and open:

`Options -> Preferences -> Audio -> Device`

Select:

- Audio system: `ASIO`
- ASIO Driver: `Sound Blaster X4 ARM64 ASIO`

Keep the first test at 48 kHz / stereo / 512 frames.

Proof goals, in order:

1. REAPER lists the driver.
2. REAPER selects it without DLL/COM load error.
3. output channels 1/2 are visible.
4. transport starts.
5. an actual REAPER project or test signal is audible through X4.
6. stop, start again, then close REAPER without a crash or stuck X4 stream.

Do not run normal Windows playback through X4 concurrently with this first ASIO proof. The coexistence gate should refuse BUSY instead of forcing a second render pin.

### 4. Remove if needed

```bat
uninstall_b4d.cmd
```

Expected:

```text
B4D UNREGISTER RESULT: PASS
```

## Build validation

The B4D GitHub Actions build must use the Visual Studio `ARM64EC` platform, not Classic ARM64.

For final PE files, raw `0x8664` alone is not sufficient because pure x64 also uses that machine value. The workflow additionally checks the Microsoft linker header dump for `ARM64X`, which identifies the linked ARM64EC/x64-compatible image.

The workflow validates all four executable images:

- ARM64EC ASIO DLL
- ARM64EC registry-free smoke
- ARM64EC registration helper
- ARM64EC registered-host probe

## Still excluded

Even if REAPER playback succeeds, do not immediately mix in:

- input/capture
- 24-bit
- multichannel
- additional sample rates
- dynamic buffer sizes
- MMCSS/AVRT tuning
- time code
- direct monitoring
- Creative runtime dependencies
- custom kernel driver

Those are post-first-REAPER expansions.

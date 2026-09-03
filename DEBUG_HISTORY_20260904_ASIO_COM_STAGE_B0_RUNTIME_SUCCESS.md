# DEBUG HISTORY — ASIO COM Stage B0 runtime success

Date: 2026-09-04 KST

## Status

**PASS — native ARM64 ASIO COM shell ABI is hardware-host validated without touching the X4 audio stack.**

Source branch:

`exp/windows-arm64-asio-com-stage-b0`

Validated source HEAD:

`53a1854167447338ca45606b6de2181ae6d8148d`

GitHub Actions build:

- native ARM64 DLL PE machine `0xAA64`
- native ARM64 smoke EXE PE machine `0xAA64`
- build completed successfully after adding the Windows SDK COM declarations and `uuid.lib`

Built SHA-256 values from the successful Actions run:

- `x4-asio-arm64.dll`: `3A9999D92EF865094EF8F3C45BF6CB58E21E4B65955D8C98E75A4B1A8F255E89`
- `x4-asio-stage-b0-smoke.exe`: `71A76460920FD24845C0958F1F91EAAE8A71A3947BA302E060670E56DECDDAE8`

## Runtime result

User executed the registry-free smoke harness on Windows ARM64:

```text
Sound Blaster X4 ARM64 ASIO Stage B0 COM smoke
SAFETY: no registry writes; no KS open; no KsCreatePin; no WaveRT; no hardware I/O.
Loading C:\SB\x4-asio-arm64.dll
DllGetClassObject hr=0x00000000
IClassFactory::CreateInstance hr=0x00000000
init=1
driverName=Sound Blaster X4 ARM64
driverVersion=100
getChannels=0 inputs=0 outputs=2
getBufferSize=0 min=512 max=512 preferred=512 granularity=0
getSampleRate=0 rate=48000.0
start=-997 (Stage B0 expected ASE_InvalidMode=-997)
errorMessage=Stage B0 COM shell: streaming engine not connected yet
DllCanUnloadNow hr=0x00000000
STAGE B0 COM SMOKE RESULT: PASS
```

## Proven by this run

The independent native ARM64 DLL successfully supports the current Stage B0 COM/ASIO shell path:

`LoadLibrary`
→ `DllGetClassObject`
→ `IClassFactory::CreateInstance`
→ ASIO vtable calls
→ object release
→ `DllCanUnloadNow == S_OK`

The following metadata methods returned the expected frozen Stage B0 values:

- driver name: `Sound Blaster X4 ARM64`
- driver version: `100`
- channels: 0 input / 2 output
- buffer size: fixed 512 frames
- sample rate: 48 kHz

`start()` intentionally returned `ASE_InvalidMode (-997)` because Stage B0 contains no streaming engine.

## Safety conclusion

This test performed:

- no registry writes
- no KS filter open
- no `KsCreatePin`
- no WaveRT allocation/state transition
- no X4 hardware I/O

Therefore it establishes the COM/vtable layer independently of the previously isolated active-playback collision.

## Next variable

Stage B1 adds only the already hardware-proven read-only coexistence preflight inside the COM DLL `init()` path:

1. discover X4 `msft_wave`
2. open filter only
3. query Render Pin 1 `KSPROPERTY_PIN_CINSTANCES`
4. query Render Pin 1 `KSPROPERTY_PIN_GLOBALCINSTANCES`
5. immediately close filter
6. return `init=1` when FREE
7. return `init=0` with a clear BUSY error when saturated

Stage B1 must still contain **no `KsCreatePin`, no WaveRT buffer, and no KS state changes**.

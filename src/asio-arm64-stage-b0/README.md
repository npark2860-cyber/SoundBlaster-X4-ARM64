# Sound Blaster X4 native ARM64 ASIO — Stage B0

Stage B0 validates only the Windows ARM64 ASIO COM ABI shell.

## Safety scope

This stage does **not**:

- open the X4 KS filter
- call `KsCreatePin`
- allocate a WaveRT buffer
- change KS state
- access the X4 hardware
- register itself unless `DllRegisterServer` is explicitly invoked

The supplied smoke executable loads the DLL directly with `LoadLibraryW`, obtains `DllGetClassObject`, creates the ASIO object using the driver's own CLSID as the requested interface ID, and calls metadata/capability methods only.

## Independent CLSID

`{0AA6D99C-4AF6-45EF-9CCA-10AC9239B7D4}`

Creative's CLSID is deliberately not reused.

## Expected smoke result

Run from a directory containing both files:

```text
x4-asio-arm64.dll
x4-asio-stage-b0-smoke.exe
```

Then:

```bat
x4-asio-stage-b0-smoke.exe
```

Expected important lines:

```text
DllGetClassObject hr=0x00000000
IClassFactory::CreateInstance hr=0x00000000
init=1
driverName=Sound Blaster X4 ARM64
getChannels=0 inputs=0 outputs=2
getBufferSize=0 min=512 max=512 preferred=512 granularity=0
getSampleRate=0 rate=48000.0
start=-997 (Stage B0 expected ASE_InvalidMode=-997)
DllCanUnloadNow hr=0x00000000
STAGE B0 COM SMOKE RESULT: PASS
```

`start()` intentionally returns `ASE_InvalidMode` because the WaveRT engine is not connected in Stage B0.

## Registration

The DLL exports `DllRegisterServer` / `DllUnregisterServer` for the later host-registration test. Do not register Stage B0 until the registry-free smoke test passes.

When registration is eventually tested, the intended standard ASIO key is:

`HKLM\SOFTWARE\ASIO\Sound Blaster X4 ARM64 ASIO`

with the independent CLSID above.

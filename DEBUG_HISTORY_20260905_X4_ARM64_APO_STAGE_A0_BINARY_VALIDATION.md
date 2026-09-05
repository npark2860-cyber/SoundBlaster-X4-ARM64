# DEBUG HISTORY — 2026-09-05 X4 ARM64 APO Stage A0 Binary Validation

Branch:

`exp/windows-arm64-x4-native-controller`

## Supplied built artifact

File:

`X4ApoArm64.dll`

Observed size:

`176640` bytes

SHA-256:

`136aaa68e83a952e19b786526dae76ce026b3641b8cf84f13bbbe9df9152abcd`

## PE architecture

Offline inspection of the supplied build artifact confirms:

- PE32+ DLL;
- COFF machine ARM64 / `0xAA64`;
- LLVM identifies the file as `coff-arm64`;
- no CLR runtime header;
- no x64 Creative binary payload is embedded as the executable image architecture.

## COM exports

Export table contains exactly the expected Stage A0 COM entry points:

- `DllCanUnloadNow`
- `DllGetClassObject`

`DllRegisterServer` is intentionally absent because Stage A0 does not use self-registration.

## Official SB1815/X4 CLSID presence

The exact little-endian binary GUID values are physically present in the built DLL for:

- SFX `{71DAB6A1-39F3-423E-90A8-032729851157}`
- MFX `{C624D7B2-8333-448E-85C8-51EEFC2025ED}`
- EFX `{EC2F4B76-6AE1-4DB9-8FF6-344B74CF9650}`

This confirms the built image contains the intended official X4/SB1815 class identities.

## AVRT sections

The final linked image contains:

- `RT_CODE`
- `RT_CONST`
- `RT_DATA`

This confirms the AVRT real-time placement directives survived compilation/linking into the binary.

## Imports

Observed import DLLs are limited to normal Windows/MSVC runtime dependencies:

- `KERNEL32.dll`
- `ole32.dll`
- `OLEAUT32.dll`
- `USER32.dll`
- `VCRUNTIME140.dll`
- Universal CRT API-set DLLs

No `CTUSBAPO64.dll`, `CTUSBWrap64.dll`, `CTUSBDGFX64.dll`, Creative kernel filter or x64 Creative runtime DLL is imported by Stage A0.

## Friendly-name strings

The final image includes the expected registration-property strings:

- `Sound Blaster X4 ARM64 Pass-through SFX`
- `Sound Blaster X4 ARM64 Pass-through MFX`
- `Sound Blaster X4 ARM64 Pass-through EFX`

## Gate result

The **binary build/architecture/export gate passes**.

This does not yet prove:

- ATL class factory creation works at runtime;
- all expected APO interfaces QI successfully;
- AudioDG can instantiate the APO;
- endpoint binding/package installation works.

## Next gate — offline COM probe

A new ARM64 probe was added at:

`src/x4-apo-com-probe-arm64`

It performs only:

`LoadLibraryW -> DllGetClassObject -> IClassFactory::CreateInstance -> QueryInterface`

for all three SFX/MFX/EFX classes.

It does not register the DLL, call APO Initialize, touch an endpoint, issue CTCDC commands or write effect properties.

Do not proceed to live APO installation or endpoint binding until the offline COM probe reports `RESULT: PASS`.

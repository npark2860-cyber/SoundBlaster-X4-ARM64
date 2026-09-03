# DEBUG HISTORY — Windows CTCDC Path (2026-09-03)

This document records the Windows ARM64 control-path investigation that led from rejected transport guesses to the current CTCDC unlock/session hypothesis.

## 1. Windows-local transport correction

Early Windows work incorrectly carried the Android BLE transport into Windows.

That was rejected because the Windows controller target is the X4 connected locally over USB.

Rule established:

- Android BLE is useful for protocol discovery.
- Windows implementation must use the locally attached USB device stack.

The old Windows BLE PoC was removed from `main`.

## 2. Hardware-confirmed USB function layout

Read-only Windows ARM64 diagnostics identified:

- composite parent `USB\VID_041E&PID_3278`
- MI_00 HID / Consumer Control
- MI_01 USB serial (`usbser`, COM3)
- MI_03 USB Audio 2.0 (`usbaudio2` on the ARM64 machine)

A raw USB configuration descriptor dump later showed seven interfaces:

- IF0 HID
- IF1 CDC ACM control
- IF2 CDC data, bulk OUT `0x03`, bulk IN `0x82`
- IF3 UAC2 AudioControl
- IF4–6 UAC2 AudioStreaming

Descriptor summary:

- `wTotalLength = 1143`
- vendor-class interface descriptors = `0`
- UAC Extension Unit candidates = `0`

This ruled out the simple theory that a hidden vendor-specific USB interface or UAC Extension Unit was the obvious Windows control channel.

## 3. Direct Mode protocol itself remained valid

The Android hardware test had already established:

- OFF `5A 39 03 00 05 00`
- ON  `5A 39 03 00 05 01`

Later Windows managed-code analysis independently produced the same frame:

- command `0x39` (`FeatureControl`)
- payload operation `0`
- feature bit position `5` from mask `0x20`
- value `0/1`
- `RawCmd5A<T>` header `0x5A`

Therefore transport/session setup, not the Direct Mode command bytes, became the primary suspect.

## 4. Raw COM3 write — negative runtime result

Branch:

`poc/windows-arm64-usb-serial-direct-mode`

The ARM64 PoC:

- located `VID_041E&PID_3278&MI_01`
- opened COM3
- wrote exactly six raw bytes for Direct Mode ON/OFF

Windows reported successful writes.

Physical X4 result:

**No reaction.**

This eliminated “open COM3 and immediately write the known MIDAS frame” as a complete Windows implementation.

## 5. HID output — negative runtime result

Read-only HID capabilities showed MI_00 as Consumer Control with:

- input report length 65
- output report length 65
- feature report length 0
- one 64-byte output value field behind the report-ID byte

A test branch sent a 65-byte report containing the exact known Direct Mode frame at bytes 1..6.

Both methods were accepted by Windows:

- `HidD_SetOutputReport`
- `WriteFile`

Physical X4 result:

**No reaction.**

Conclusion: do not continue blind HID framing guesses.

## 6. Windows Audio DeviceTopology — no proprietary control surfaced

A read-only topology diagnostic enumerated render/capture paths and exposed normal Windows audio topology nodes such as:

- volume
- mute
- mixer / SuperMix
- selector
- ADC
- DAC

No useful Creative proprietary/DEV_SPECIFIC node emerged from DeviceTopology.

This prompted raw USB descriptor inspection, which also found no vendor-class interface or UAC Extension Unit.

## 7. Creative online SB1815 package recovered

The Creative App's downloaded SB1815 package was recovered.

Package marker:

- PackageId `SB1815`
- PackageName `SB1815`
- Version `1.0.10.00`

`DrvUpdate.exe` metadata:

- Creative Sound Blaster (CT)USB Audio Drivers Setup
- version `3.06.00.00`

The Inno payload was extracted offline without installing the driver package.

67 files were recovered and all 67 matched the package-recorded SHA-1 checksums.

Important files:

- `ctusbaud.inf`
- `CTUSBfilt64.sys`
- `CtUSBa64.sys`
- `CTUSBWrap64.dll`
- `CTUSBAPO64.dll`
- `CTUSBDGFX64.dll`
- `CTUSBppld64.dll`
- `CTIOM64.exe`

## 8. X4 driver-stack interpretation corrected

`ctusbaud.inf` analysis showed the X4/SB1815 MI_03 path is not accurately described as “Creative replaces usbaudio2 with CtUSBa64.sys.”

For `USB\VID_041E&PID_3278&MI_03`, the package relies on the Microsoft USB Audio 2.0 path and associates Creative `CTUSBfilt64` as an upper filter for the relevant device stack.

This is compatible with the ARM64 system exposing Microsoft `usbaudio2` while the official Creative x64 package adds extra Creative behavior on top.

## 9. KS/property investigation

Static analysis of Creative driver/APO components found:

- use of `DeviceIoControl`
- `IOCTL_KS_PROPERTY` (`0x002F0003`)
- Creative property GUID candidates including:
  - `{E8E7B1C0-EB43-4AA5-98EE-7F5DB42D902F}`
  - `{2F2C8DDD-4198-4FAC-BA29-61BB05B7DE06}`

The `{E8E7...}` GUID appears in Creative APO/filter/driver components, and at least one observed call was a `KSPROPERTY_TYPE_BASICSUPPORT` query.

This path remains relevant to the Creative audio filter stack, but it did not yet directly identify the Direct Mode setter.

Do not assume every `{E8E7...}` property operation is Direct Mode.

## 10. DirectMode feature module analysis

`Creative.App.Features.DirectMode.dll` was supplied and analyzed.

Finding:

The feature DLL mainly performs UI/state coordination. Its constructor accepts:

- `IDevice`
- `IToggleFeature`

The actual ON/OFF implementation is delegated through `ToggleFeatureViewModel` / `IToggleFeature`.

The module separately manages audio-format state via `IValueFeature`, including get/set of `AudioFormat` and last selected speaker format.

Therefore the low-level control path had to be traced into `Creative.Platform.Devices.dll`.

## 11. Creative.Platform.Devices.dll — key breakthrough

User supplied:

- `Creative.Platform.Devices.dll`
- `Creative.App.UI.Framework.dll`

The Windows managed implementation constructs the exact same Direct Mode command used on Android:

`5A 39 03 00 05 value`

The call chain narrows to:

`DirectModeFeatureViewModel`
-> `ToggleFeatureViewModel`
-> `IToggleFeature`
-> Platform device feature implementation
-> `CDCConnection.RawSetValue()`
-> `CDCConnection.Write()`
-> native CTCDC layer

The native command used for raw passthrough is:

`CTCDCCMD_WritePassthroughData = 1001`

The managed side prepares a structure containing:

- pointer to the raw MIDAS frame
- byte count (`6` for Direct Mode)

and calls `ICTCDC.ExecuteCommand(1001, ...)`.

This explains why the raw command bytes can be correct while a naked COM3 write still fails.

## 12. CTCDC / CTIntrfu native layer

The user then supplied the native files:

`CTCDC.dll`

- size `2,122,200`
- SHA-256 `bc4010e8f7000bfe6217425a0622dd710a7626d90fb61008505337aa87a43dab`

`CTIntrfu.dll`

- size `109,656`
- SHA-256 `ecf098101a0663568f4a406d7bed9775565a67213930e2487c17d858a5d0d9b6`

These binaries are not committed to the repository.

The current priority is full static tracing of these exact binaries.

## 13. Preliminary CTCDC serial/session findings

Before the handoff, enough CTCDC behavior had already been reverse-engineered to create a safe ARM64 probe.

Branch:

`poc/windows-arm64-usb-serial-ctcdc-init`

HEAD at handoff:

`d44a33936639cc76c935b59c0502133eaa5bcf2d`

Observed CTCDC COM initialization reproduced by the branch:

- comm event mask `0x05` (`EV_RXCHAR | EV_TXEMPTY`)
- `115200` baud
- `8N1`
- zero COM timeouts
- `PurgeComm(0x0F)`
- `SETDTR`

First normal CTCDC protocol query:

`5A 03 00`

If a valid command-`0x03` response is not received, the preliminary trace proceeds into an unlock path whose first text greeting is:

`whoareyou.MyApp8\r\n`

The probe stops after collecting the response to that greeting.

It does **not** synthesize the cryptographic unlock reply and does **not** send Direct Mode.

Successful ARM64 CI build:

- run ID `33692026928`
- conclusion `success`

Final workflow on the branch is `workflow_dispatch` only.

No actual hardware runtime log from this probe is recorded in the repository yet.

## 14. Current hypothesis

The most evidence-supported Windows path is now:

Creative App / managed feature
-> `Creative.Platform.Devices`
-> CTIntrfu / CTCDC
-> CDC serial session initialization
-> firmware/protocol query
-> conditional unlock/challenge-response
-> software mode / session activation
-> `ExecuteCommand(1001)` passthrough
-> raw MIDAS frame such as `5A 39 03 00 05 01`
-> X4 firmware

The exact unlock response algorithm, software-mode command, response parsing, and passthrough session requirements remain to be recovered from `CTCDC.dll` / `CTIntrfu.dll`.

## 15. What is explicitly rejected

Do not repeat these without new evidence:

- Windows BLE control path
- Direct Mode `6A...` frame
- guessed `5C...` Direct Mode wrapper
- naked COM3 Direct Mode write as the final solution
- HID Output Report Direct Mode framing as already tested
- assumption that a UAC Extension Unit exists
- assumption that a vendor-class USB interface exists
- assumption that `CtUSBa64.sys` alone is the X4's complete Windows driver stack

The next phase must be driven by the native CTCDC state machine, not by transport guessing.

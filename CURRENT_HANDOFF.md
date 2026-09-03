# CURRENT HANDOFF — Sound Blaster X4 Windows ARM64

Updated: 2026-09-03 KST

## Source of truth

Repository: `npark2860-cyber/SoundBlaster-X4-ARM64`

Default branch: `main`

Main HEAD immediately before this handoff documentation commit:

`6d5780dcd38191aec85a0cea52fe0be9c34e7cfc`

Do **not** reconstruct state from old chat context. At startup, verify the actual current GitHub `main` HEAD and the branch heads below, then read:

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260903_WINDOWS_CTCDC_PATH.md`
3. `NEXT_ACTION_CTCDC.md`
4. Existing `PROTOCOL.md`, `RUNTIME_VALIDATION.md`, and `apk-analysis/*` only as supporting history.

This handoff intentionally records documentation only. Experimental code branches remain unmerged.

---

## Project goal

Build a native Windows ARM64 controller for Creative Sound Blaster X4 / SB1815 on the user's Snapdragon Windows ARM64 machine.

Critical architecture rule:

- Windows target is the **locally USB-connected X4**.
- Android/mobile BLE is useful for protocol discovery only.
- Do **not** reintroduce Bluetooth as the Windows transport.

Device identity:

- Product: `Sound Blaster X4`
- Codename: `Accent2`
- Model/package: `SB1815`
- USB VID/PID: `041E:3278`

---

## Direct Mode command — confirmed

Android hardware validation and Windows `Creative.Platform.Devices.dll` independently converge on the same raw MIDAS command.

Direct Mode OFF:

`5A 39 03 00 05 00`

Direct Mode ON:

`5A 39 03 00 05 01`

Frame:

`5A | command=39 | payload_length=03 | payload=00 05 value`

Windows static trace:

- `CDCRawCommand.FeatureControl = 0x39`
- Direct Mode feature mask = `0x20`
- bit position = `5`
- SET operation byte = `0`
- value = `0/1`
- `RawCmd5A<T>` supplies header `0x5A`

Do **not** revive obsolete Direct Mode candidates using `6A` or guessed `5C` frames.

---

## Android/mobile transport — confirmed but Windows-irrelevant

X4 BLE name: `Control for SB1815`

GATT:

- Service: `b7860001-11b8-b681-6343-5a6c2286633f`
- Write: `b7860002-11b8-b681-6343-5a6c2286633f`
- Read/Notify: `b7860003-11b8-b681-6343-5a6c2286633f`
- CCCD: `00002902-0000-1000-8000-00805f9b34fb`

Android Debug Protocol raw sender physically confirmed the Direct Mode frames above.

This BLE path must not be used as the Windows solution.

---

## Windows USB topology — hardware confirmed

Composite device:

`USB\VID_041E&PID_3278`

Raw configuration descriptor:

- total length: `1143`
- 7 USB interfaces
- no vendor-class (`0xFF`) interface
- no UAC Extension Unit

Interfaces:

- IF0: HID / Consumer Control
- IF1: CDC ACM control
- IF2: CDC data
  - Bulk OUT `0x03`
  - Bulk IN `0x82`
- IF3: USB Audio 2.0 AudioControl
- IF4–IF6: USB Audio 2.0 AudioStreaming

Windows PnP observations:

- MI_00 -> HID (`HidUsb`)
- MI_01 -> `USB Serial Device (COM3)` (`usbser`)
- MI_03 -> Sound Blaster X4 USB Audio 2.0 (`usbaudio2` on the current ARM64 machine)

DeviceTopology exposed standard volume/mute/mixer/ADC/DAC/SuperMix nodes only; no useful Creative proprietary node was exposed there.

---

## Negative runtime results — do not repeat blindly

### Raw serial Direct Mode

Branch: `poc/windows-arm64-usb-serial-direct-mode`

Current branch HEAD recorded at handoff:

`b8d763de343e87c0af101d0b7495a40ba2ddd703`

The tool opened COM3 and successfully wrote the 6-byte Direct Mode frames, but the physical X4 showed **no reaction**.

Conclusion: raw COM write without Creative's CDC initialization/unlock/software-mode sequence is insufficient.

### HID Output Direct Mode

Branch: `poc/windows-arm64-hid-output-direct-mode`

HEAD:

`5824c203f75ddf2ab0e0c5663f53ba674df68552`

Both `HidD_SetOutputReport` and `WriteFile` accepted 65-byte reports containing the known Direct Mode frame. The physical X4 showed **no reaction**.

Conclusion: MI_00 HID is not the direct Windows control transport in that form.

Do not continue blind HID prefix/report guessing.

---

## SB1815 online package / Creative Windows driver package

User recovered the Creative online SB1815 package.

Package marker:

- PackageId: `SB1815`
- PackageName: `SB1815`
- package Version: `1.0.10.00`

Inside it, `DrvUpdate.exe` is:

- `Creative Sound Blaster (CT)USB Audio Drivers Setup`
- version `3.06.00.00`
- Inno Setup package

Offline extraction recovered 67 payload files and all 67 matched their recorded SHA-1 checksums.

Important recovered files include:

- `ctusbaud.inf`
- `CTUSBfilt64.sys`
- `CtUSBa64.sys`
- `CTUSBWrap64.dll`
- `CTUSBAPO64.dll`
- `CTUSBDGFX64.dll`
- `CTUSBppld64.dll`
- `CTIOM64.exe`

Important architecture correction from `ctusbaud.inf`:

For X4/SB1815 `USB\VID_041E&PID_3278&MI_03`, Creative does not simply replace the whole USB audio stack with `CtUSBa64.sys`; the package uses Microsoft USB Audio 2.0 and attaches `CTUSBfilt64` as a Creative upper filter for the relevant X4 path.

Do not simplify the architecture to “X4 always uses CtUSBa64.sys as the main driver.”

---

## Windows Creative App Direct Mode call chain — confirmed static trace

`Creative.App.Features.DirectMode.dll` is mostly UI/state coordination. It delegates the actual toggle through `IToggleFeature`.

Relevant managed dependencies supplied by user:

- `Creative.Platform.Devices.dll`
  - SHA-256 `2d77172fb6ae850b6d03a09830892c8c3a0ab79e10dda28f40a76b3fadc47e93`
- `Creative.App.UI.Framework.dll`
  - SHA-256 `f903fc410528a314fc890df53766d19dad11efb0b5074017f3e71de4d905f8d8`

Managed call chain:

`DirectModeFeatureViewModel`
-> `ToggleFeatureViewModel`
-> `IToggleFeature`
-> `Creative.Platform.Devices`
-> `CDCConnection.RawSetValue()`
-> `CDCConnection.Write()`
-> CTCDC passthrough command

The Platform Devices code constructs the same physical Direct Mode frame already confirmed on Android.

Low-level write path:

- CTCDC command enum: `CTCDCCMD_WritePassthroughData = 1001`
- payload structure contains pointer to raw bytes + `dwSize`
- the raw payload for Direct Mode is the 6-byte `5A 39 03 00 05 value` frame
- managed code calls `ICTCDC.ExecuteCommand(1001, ...)`

`CTIntrfu.dll` is used to obtain/load the CTCDC implementation (`CTCreateInstanceEx` path observed in prior static trace).

Known COM-style identifiers from prior trace:

- CCTCDC CLSID: `{66FC4CF0-56C8-4523-A92B-CE69FCD7556A}`
- ICTCDC IID: `{669E9C0E-AD66-48C3-8228-29A55C2E9977}`

Do not treat these identifiers alone as proof of the transport mechanics; continue tracing the supplied native binaries.

---

## CTCDC.dll / CTIntrfu.dll — newest inputs

The user has now supplied the two native binaries needed for the next phase:

- `CTCDC.dll`
  - size: `2,122,200` bytes
  - SHA-256: `bc4010e8f7000bfe6217425a0622dd710a7626d90fb61008505337aa87a43dab`
- `CTIntrfu.dll`
  - size: `109,656` bytes
  - SHA-256: `ecf098101a0663568f4a406d7bed9775565a67213930e2487c17d858a5d0d9b6`

These proprietary binaries are **not committed to GitHub**. Only their identity/hashes are recorded here.

If a new chat cannot access the prior conversation files, ask the user to re-upload exactly these two DLLs rather than searching for replacements.

---

## CTCDC preliminary reverse-engineering state

A preliminary static trace already established enough CTCDC behavior to build a controlled probe.

Experimental branch:

`poc/windows-arm64-usb-serial-ctcdc-init`

Branch HEAD at handoff:

`d44a33936639cc76c935b59c0502133eaa5bcf2d`

Workflow is currently manual-only (`workflow_dispatch`).

Successful ARM64 CI build:

- workflow: `Build X4 CTCDC Serial Probe ARM64`
- run ID: `33692026928`
- result: success

Probe source reproduces CTCDC's observed serial initialization:

- event mask `0x05` = `EV_RXCHAR | EV_TXEMPTY`
- baud `115200`
- 8 data bits
- no parity
- one stop bit
- zero COM timeouts
- `PurgeComm(0x0F)`
- `SETDTR`

Then it sends CTCDC's first normal protocol probe:

`5A 03 00`

If no valid command-`0x03` reply is received, it sends only the first observed unlock greeting:

`whoareyou.MyApp8\r\n`

The probe intentionally stops before the cryptographic unlock reply and does not send Direct Mode.

**No hardware runtime result for this CTCDC probe is recorded in GitHub at this handoff.** Do not invent one.

---

## Current branch inventory

At handoff time:

- `main` -> `6d5780dcd38191aec85a0cea52fe0be9c34e7cfc` before this documentation commit
- `diag/windows-arm64-audio-topology` -> `aa364102cd3d31aec5bc8b1844971466609fbe03`
- `diag/windows-arm64-hid-caps` -> `348445524cf90cefb6649297949eab8e510b1eb1`
- `diag/windows-arm64-usb-local` -> `3c0786b73cc1d25745e196ef6b9883253ce39b3e`
- `poc/windows-arm64-direct-mode` -> `6b7fbf407caeac7345c69751e8957efec52cb360`
- `poc/windows-arm64-hid-output-direct-mode` -> `5824c203f75ddf2ab0e0c5663f53ba674df68552`
- `poc/windows-arm64-usb-serial-direct-mode` -> `b8d763de343e87c0af101d0b7495a40ba2ddd703`
- `poc/windows-arm64-usb-serial-ctcdc-init` -> `d44a33936639cc76c935b59c0502133eaa5bcf2d`

Verify these against GitHub before resuming because branch heads can move.

---

## Next task

The next chat must start with the supplied `CTCDC.dll` and `CTIntrfu.dll` static analysis.

Primary goal:

Recover the exact CTCDC session state machine before Direct Mode passthrough is accepted:

1. serial initialization
2. `5A 03 00` firmware/protocol query and expected response
3. unlock decision
4. `whoareyou.MyApp8\r\n`
5. challenge/response algorithm and exact reply bytes
6. software-mode transition
7. any post-unlock/session initialization
8. `ExecuteCommand(1001)` passthrough write/read framing
9. only after that, reproduce `5A 39 03 00 05 00/01` on ARM64

Do not jump back to BLE, HID guessing, or naked COM writes.

See `NEXT_ACTION_CTCDC.md` for the execution order.
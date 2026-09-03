# CURRENT HANDOFF — Sound Blaster X4 Windows ARM64

Updated: 2026-09-03 KST

## Source of truth

Repository: `npark2860-cyber/SoundBlaster-X4-ARM64`

Default branch: `main`

Main HEAD immediately before this handoff update commit:

`fe113854dd28db8bb2a46507779560d055127366`

Do **not** reconstruct state from old chat context. At startup, verify actual GitHub `main` and branch heads, then read in this order:

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260903_WINDOWS_CTCDC_PATH.md`
3. `DEBUG_HISTORY_20260903_CTCDC_NATIVE_UNLOCK_TRACE.md`
4. `DEBUG_HISTORY_20260903_CTCDC_MAX_PAYLOAD_RUNTIME.md`
5. `NEXT_ACTION_CTCDC.md`

Experimental code branches remain unmerged.

---

## Project goal

Build a native Windows ARM64 controller for Creative Sound Blaster X4 / SB1815 on the user's Snapdragon Windows ARM64 machine.

Critical architecture rule:

- Windows target is the **locally USB-connected X4**.
- Android BLE was useful for protocol discovery only.
- Do not reintroduce Bluetooth as the Windows transport.

Device:

- Sound Blaster X4
- codename `Accent2`
- package/model `SB1815`
- USB VID/PID `041E:3278`

---

## Current stage

### Complete

- native `CTCDC.dll` / `CTIntrfu.dll` static analysis
- existing ARM64 probe audit against the binary
- first real CTCDC runtime readiness query

### Hardware-confirmed breakthrough

The physical X4 replied to:

`5A 03 00`

with:

`5A 03 02 3B 00`

Binary-confirmed meaning:

- command `0x03` = `CTCDCCMD_GetMaximumPayloadSize`
- payload = `0x003B`
- Maximum Payload Size = **59 bytes**

This proves the initial CTCDC readiness query succeeds on the current X4 runtime state.

Therefore the actual `ICTCDC::Open()` path for this state is:

`serial init`
→ `5A 03 00`
→ valid max-payload response
→ **skip Unlock**
→ **skip `SW_MODE1`**
→ `5A 09 01 02` GetFirmwareVersionString
→ `5A 26 01 05` QueryButtonsAvailable
→ normal session

The unlock algorithm remains a contingency path only if a future state fails the initial maximum-payload query.

---

## Current next task

Run the Stage C2 read-only Open-session validation probe.

Branch:

`poc/windows-arm64-usb-serial-ctcdc-init`

Current branch HEAD:

`1b95f0734e2a2a26d9bc606809b9e04942d1808a`

Workflow:

`Build X4 CTCDC Serial Probe ARM64`

Trigger:

`workflow_dispatch` only

The branch now builds `session-open-probe.cpp`.

The probe performs only:

1. CTCDC serial setup
2. `5A 03 00` GetMaximumPayloadSize
3. `5A 09 01 02` GetFirmwareVersionString
4. `5A 26 01 05` QueryButtonsAvailable
5. stop

It does not send an unlock response, `SW_MODE1`, Direct Mode, or unknown commands.

Capture:

`x4-ctcdc-probe.txt`

See `NEXT_ACTION_CTCDC.md`.

---

## Direct Mode command — fixed and confirmed

OFF:

`5A 39 03 00 05 00`

ON:

`5A 39 03 00 05 01`

Independent evidence:

- Android hardware validation
- Windows `Creative.Platform.Devices.dll` managed static analysis

Construction:

- command `0x39` = FeatureControl
- SET operation = `0`
- Direct Mode feature bit position = `5`
- value = `0/1`
- header = `0x5A`

Do not revive `6A` or guessed `5C` variants.

---

## Windows USB topology — hardware confirmed

Composite device:

`USB\VID_041E&PID_3278`

Relevant interfaces:

- MI_00 / IF0: HID Consumer Control
- MI_01 / IF1+IF2: CDC ACM + CDC data, exposed as COM3
- MI_03 / IF3: USB Audio 2.0 AudioControl
- IF4–IF6: USB AudioStreaming

CDC endpoints:

- Bulk OUT `0x03`
- Bulk IN `0x82`

Descriptor facts:

- total length `1143`
- seven interfaces
- no vendor-class `0xFF` interface
- no UAC Extension Unit

---

## Eliminated paths — do not repeat

### Naked COM Direct Mode

Branch:

`poc/windows-arm64-usb-serial-direct-mode`

HEAD:

`b8d763de343e87c0af101d0b7495a40ba2ddd703`

The exact six Direct Mode bytes were written successfully but caused no physical X4 reaction.

### HID Output Direct Mode

Branch:

`poc/windows-arm64-hid-output-direct-mode`

HEAD:

`5824c203f75ddf2ab0e0c5663f53ba674df68552`

Both tested Windows HID write methods were accepted but caused no physical X4 reaction.

Do not return to BLE, HID guessing, vendor-interface hunting, UAC Extension Unit hunting, naked COM Direct Mode writes, `6A`, or guessed `5C` wrappers.

---

## Creative Windows control call chain

Managed path:

`DirectModeFeatureViewModel`
→ `ToggleFeatureViewModel`
→ `IToggleFeature`
→ `Creative.Platform.Devices`
→ `CDCConnection.RawSetValue()`
→ `CDCConnection.Write()`
→ `ICTCDC.ExecuteCommand(1001, ...)`

`1001` = `CTCDCCMD_WritePassthroughData`.

Native static analysis confirms command 1001 writes the supplied raw payload without adding another MIDAS wrapper.

Therefore, once CTCDC session setup is validated, Direct Mode remains exactly the six-byte frame above.

---

## Native binary identities

### CTCDC.dll

- size `2,122,200`
- SHA-256 `bc4010e8f7000bfe6217425a0622dd710a7626d90fb61008505337aa87a43dab`
- PE32/x86

### CTIntrfu.dll

- size `109,656`
- SHA-256 `ecf098101a0663568f4a406d7bed9775565a67213930e2487c17d858a5d0d9b6`
- PE32/x86

These proprietary binaries are reference inputs and are not committed to the repository.

If unavailable in a future chat, re-upload these exact binaries rather than substituting other versions.

---

## CTIntrfu / CTCDC native trace summary

Factory path:

`CTCreateInstanceEx`
→ Creative component resolution
→ `LoadLibraryExW`
→ `DllGetClassObject`
→ `IClassFactory::CreateInstance`
→ `ICTCDC`

Confirmed IDs:

- CCTCDC CLSID `{66FC4CF0-56C8-4523-A92B-CE69FCD7556A}`
- ICTCDC IID `{669E9C0E-AD66-48C3-8228-29A55C2E9977}`

Relevant methods:

- Initialize VA `0x10003430`
- Open VA `0x100035F0`
- ExecuteCommand VA `0x10003BE0`
- Close VA `0x10003DF0`
- Shutdown VA `0x10003EE0`

Serial setup:

- event mask `0x05`
- `115200`
- `8N1`
- DTR/RTS control disabled while unrelated DCB flags are preserved
- zero COM timeouts
- `PurgeComm(0x0F)`
- `SETDTR`

---

## Unlock path — contingency only

If a future runtime state fails `5A 03 00`, CTCDC enters the recovered unlock path.

Greeting:

`whoareyou.MyApp8\r\n`

Normal challenge:

- prefix `whoareyou`
- seed4
- challenge32

Effective key:

`seed[0:2] || D3 1A 21 27 9B E3 46 F0 99 9D 6E C4 C3 FE BE 98 90 18 69 C1 18 FB B1 25 6E 0C E0 7B || seed[2:4]`

Cipher:

- AES-256-GCM
- no AAD observed
- first 12 bytes of transmitted random16 are nonce
- ciphertext32
- tag16

Response:

`"unlock" || random16 || ciphertext32 || tag16 || "\r\n"`

Expected success:

`unlock_OK\r\n`

Then:

`SW_MODE1\r\n`
→ `5A 03 00`
→ `5A 09 01 02`
→ `5A 26 01 05`

Do not force this path while the initial maximum-payload query succeeds.

---

## Creative driver-stack note

The official SB1815 package shows X4 MI_03 on Microsoft USB Audio 2.0 with Creative `CTUSBfilt64` attached as an upper filter for the relevant Creative path.

Do not simplify this to “X4 always uses `CtUSBa64.sys` as the main driver.”

The current Direct Mode path under investigation is CTCDC/CDC, not a guessed audio-class vendor control.

---

## Current branch inventory

At this handoff update:

- `main` -> `fe113854dd28db8bb2a46507779560d055127366` immediately before this handoff commit
- `diag/windows-arm64-audio-topology` -> `aa364102cd3d31aec5bc8b1844971466609fbe03`
- `diag/windows-arm64-hid-caps` -> `348445524cf90cefb6649297949eab8e510b1eb1`
- `diag/windows-arm64-usb-local` -> `3c0786b73cc1d25745e196ef6b9883253ce39b3e`
- `poc/windows-arm64-direct-mode` -> `6b7fbf407caeac7345c69751e8957efec52cb360`
- `poc/windows-arm64-hid-output-direct-mode` -> `5824c203f75ddf2ab0e0c5663f53ba674df68552`
- `poc/windows-arm64-usb-serial-direct-mode` -> `b8d763de343e87c0af101d0b7495a40ba2ddd703`
- `poc/windows-arm64-usb-serial-ctcdc-init` -> `1b95f0734e2a2a26d9bc606809b9e04942d1808a`

Verify actual GitHub heads before resuming.

# CURRENT HANDOFF — Sound Blaster X4 Windows ARM64

Updated: 2026-09-03 KST

## Source of truth

Repository: `npark2860-cyber/SoundBlaster-X4-ARM64`

Default branch: `main`

Main HEAD immediately before this handoff update commit:

`36145b921ce6ff47f6414fb326c03a3b4fb8b994`

Do **not** reconstruct state from old chat context. At startup, verify actual GitHub `main` and branch heads, then read in this order:

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260903_WINDOWS_CTCDC_PATH.md`
3. `DEBUG_HISTORY_20260903_CTCDC_NATIVE_UNLOCK_TRACE.md`
4. `NEXT_ACTION_CTCDC.md`

`PROTOCOL.md`, `RUNTIME_VALIDATION.md`, and `apk-analysis/*` are supporting history only.

Experimental code branches remain unmerged.

---

## Project goal

Build a native Windows ARM64 controller for Creative Sound Blaster X4 / SB1815 on the user's Snapdragon Windows ARM64 machine.

Critical architecture rule:

- Windows target is the **locally USB-connected X4**.
- Android/mobile BLE was useful for protocol discovery only.
- Do not reintroduce Bluetooth as the Windows transport.

Device:

- Product: `Sound Blaster X4`
- Codename: `Accent2`
- Model/package: `SB1815`
- USB VID/PID: `041E:3278`

---

## Current stage

Native `CTCDC.dll` / `CTIntrfu.dll` static analysis is now **complete for the current input hashes**.

Stage A — native static analysis: **complete**

Stage B — audit/fix existing safe ARM64 CTCDC probe against native binary: **complete**

Stage C — controlled hardware challenge capture: **next**

Do not repeat Stage A/B unless the binaries or branch state change.

Full static trace:

`DEBUG_HISTORY_20260903_CTCDC_NATIVE_UNLOCK_TRACE.md`

Execution instructions:

`NEXT_ACTION_CTCDC.md`

---

## Direct Mode command — confirmed

The raw MIDAS command is fixed and already independently confirmed by Android hardware validation and Windows managed-code analysis.

OFF:

`5A 39 03 00 05 00`

ON:

`5A 39 03 00 05 01`

Windows construction:

- command `0x39` = `FeatureControl`
- SET operation = `0`
- Direct Mode feature bit position = `5` from mask `0x20`
- value = `0/1`
- frame header = `0x5A`

Do not revive obsolete `6A` or guessed `5C` Direct Mode variants.

---

## Windows USB topology — hardware confirmed

Composite parent:

`USB\VID_041E&PID_3278`

Relevant interfaces:

- MI_00 / IF0: HID Consumer Control
- MI_01 / IF1+IF2: CDC ACM + data, exposed as `USB Serial Device (COM3)` / `usbser`
  - Bulk OUT `0x03`
  - Bulk IN `0x82`
- MI_03 / IF3: USB Audio 2.0 AudioControl
- IF4–IF6: USB AudioStreaming

Raw descriptor facts:

- total configuration length `1143`
- seven interfaces
- no vendor-class `0xFF` interface
- no UAC Extension Unit

DeviceTopology exposed only normal Windows audio nodes; no useful Creative proprietary control node emerged there.

---

## Eliminated runtime paths

### Naked COM Direct Mode

Branch:

`poc/windows-arm64-usb-serial-direct-mode`

HEAD:

`b8d763de343e87c0af101d0b7495a40ba2ddd703`

Writing the exact six Direct Mode bytes to COM3 succeeded at the OS level but produced **no physical X4 reaction**.

Conclusion: the command bytes are correct, but CTCDC session initialization is required.

### HID Output Direct Mode

Branch:

`poc/windows-arm64-hid-output-direct-mode`

HEAD:

`5824c203f75ddf2ab0e0c5663f53ba674df68552`

Both tested HID write methods were accepted by Windows but produced **no physical X4 reaction**.

Do not continue HID prefix/report guessing.

---

## Creative Windows control call chain

Managed static trace:

`DirectModeFeatureViewModel`
→ `ToggleFeatureViewModel`
→ `IToggleFeature`
→ `Creative.Platform.Devices`
→ `CDCConnection.RawSetValue()`
→ `CDCConnection.Write()`
→ `ICTCDC.ExecuteCommand(1001, ...)`

`1001` = `CTCDCCMD_WritePassthroughData`.

The native implementation adds **no additional MIDAS wrapper**. After a valid CTCDC session is established, command 1001 writes the caller's raw bytes to the CDC path.

Known COM identities, now confirmed in the native binary:

- CCTCDC CLSID `{66FC4CF0-56C8-4523-A92B-CE69FCD7556A}`
- ICTCDC IID `{669E9C0E-AD66-48C3-8228-29A55C2E9977}`

---

## Exact native inputs

`CTCDC.dll`

- size `2,122,200`
- SHA-256 `bc4010e8f7000bfe6217425a0622dd710a7626d90fb61008505337aa87a43dab`
- PE32 / x86

`CTIntrfu.dll`

- size `109,656`
- SHA-256 `ecf098101a0663568f4a406d7bed9775565a67213930e2487c17d858a5d0d9b6`
- PE32 / x86

These proprietary binaries are not committed to GitHub. If unavailable in a future chat, re-upload these exact binaries rather than substituting other versions.

---

## Native CTCDC state machine — binary confirmed

Factory/load path:

`CTCreateInstanceEx`
→ Creative component resolution
→ `LoadLibraryExW`
→ `DllGetClassObject`
→ `IClassFactory::CreateInstance`
→ `ICTCDC`

Relevant ICTCDC methods:

- `Initialize` VA `0x10003430`
- `Open` VA `0x100035F0`
- `ExecuteCommand` VA `0x10003BE0`
- `Close` VA `0x10003DF0`
- `Shutdown` VA `0x10003EE0`

Serial setup performed by CTCDC:

- event mask `0x05` = `EV_RXCHAR | EV_TXEMPTY`
- 115200 baud
- 8N1
- DTR/RTS control-mode bits disabled while unrelated DCB flags are preserved
- all COM timeouts zeroed
- `PurgeComm(0x0F)`
- `SETDTR`

### Important semantic correction

`5A 03 00` is **not** a firmware query.

It is `CTCDCCMD_GetMaximumPayloadSize`.

A valid command-`0x03` response with a two-byte payload returns Maximum Payload Size. CTCDC uses this as an initial session-readiness test.

If it succeeds immediately, `Open()` skips unlock and `SetSwMode1`.

If it fails, CTCDC enters the unlock path.

---

## Unlock path — binary confirmed

Initial TX:

`whoareyou.MyApp8\r\n`

Normal challenge layout:

- bytes `0..8`: `whoareyou`
- bytes `9..12`: 4-byte seed
- bytes `13..44`: 32-byte challenge

Effective AES-256 key:

`seed[0:2] || D3 1A 21 27 9B E3 46 F0 99 9D 6E C4 C3 FE BE 98 90 18 69 C1 18 FB B1 25 6E 0C E0 7B || seed[2:4]`

Cipher:

- AES-256-GCM
- 32-byte challenge encrypted
- no AAD observed
- 16-byte generated random field is transmitted
- first 12 random bytes are the GCM nonce
- 16-byte GCM tag

Exact normal response layout, total 72 bytes:

`"unlock" || random16 || ciphertext32 || tag16 || "\r\n"`

Expected success response:

`unlock_OK\r\n`

Special replies handled by CTCDC include:

- `Unknown command\r\n` — old-firmware/no-unlock-required path
- `NotYet\r\n` — delayed retry path

---

## Software mode and post-unlock Open sequence

After successful unlock:

1. TX `SW_MODE1\r\n`
2. require its command-`0x02` / selector-`0x6D` success response
3. re-run `5A 03 00` GetMaximumPayloadSize
4. required firmware-version-string query:
   - TX `5A 09 01 02`
5. capability query:
   - TX `5A 26 01 05` (`QueryButtonsAvailable`)
6. normal session/callback setup continues

The firmware-version-string query is on the required `Open()` success path. The button query is not observed to gate overall Open success.

---

## Current safe ARM64 probe

Branch:

`poc/windows-arm64-usb-serial-ctcdc-init`

Current HEAD:

`2125308b869fae21cef3d074de1e7a7a0e250b27`

Workflow:

`Build X4 CTCDC Serial Probe ARM64`

Workflow trigger remains manual-only:

`workflow_dispatch`

Stage B changes on this branch only:

- corrected `5A 03 00` interpretation/logging to Maximum Payload Size
- corrected DCB handling to preserve unrelated flags
- added `GetCommTimeouts` before zeroing
- updated probe README

No unlock response, `SW_MODE1`, or Direct Mode command was added.

The probe remains intentionally safe and stops after collecting the unlock-stage response.

No hardware runtime log from this updated CTCDC probe is recorded yet.

---

## Creative driver-stack note

The recovered official SB1815 package shows that X4 MI_03 uses the Microsoft USB Audio 2.0 path with Creative `CTUSBfilt64` attached as an upper filter for the relevant Creative stack.

Do not simplify this to “X4 always uses `CtUSBa64.sys` as the main driver.”

This is supporting architecture context; the current Direct Mode path is the CTCDC session described above.

---

## Current branch inventory

At this handoff update:

- `main` -> `36145b921ce6ff47f6414fb326c03a3b4fb8b994` immediately before this handoff commit
- `diag/windows-arm64-audio-topology` -> `aa364102cd3d31aec5bc8b1844971466609fbe03`
- `diag/windows-arm64-hid-caps` -> `348445524cf90cefb6649297949eab8e510b1eb1`
- `diag/windows-arm64-usb-local` -> `3c0786b73cc1d25745e196ef6b9883253ce39b3e`
- `poc/windows-arm64-direct-mode` -> `6b7fbf407caeac7345c69751e8957efec52cb360`
- `poc/windows-arm64-hid-output-direct-mode` -> `5824c203f75ddf2ab0e0c5663f53ba674df68552`
- `poc/windows-arm64-usb-serial-direct-mode` -> `b8d763de343e87c0af101d0b7495a40ba2ddd703`
- `poc/windows-arm64-usb-serial-ctcdc-init` -> `2125308b869fae21cef3d074de1e7a7a0e250b27`

Verify actual GitHub heads before resuming.

---

## Next task — Stage C only

Build the current `poc/windows-arm64-usb-serial-ctcdc-init` branch manually with its existing `workflow_dispatch` workflow and run the resulting ARM64 probe against the USB-connected X4.

Capture and preserve:

`x4-ctcdc-probe.txt`

Required evidence:

1. exact bytes returned to `5A 03 00`
2. whether a valid maximum-payload response is already available
3. if not, whether unlock greeting is reached
4. exact unlock-stage response bytes
5. if normal `whoareyou` challenge is returned, exact seed4 + challenge32

Do **not** send the generated AES-GCM unlock reply yet. First record the real challenge as hardware evidence.

After that capture, proceed to Stage D in `NEXT_ACTION_CTCDC.md`.

Do not jump back to BLE, HID guessing, naked COM Direct Mode writes, vendor-interface searches, UAC Extension Unit searches, `6A`, or guessed `5C` frames.

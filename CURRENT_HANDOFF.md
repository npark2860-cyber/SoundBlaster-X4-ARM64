# CURRENT HANDOFF — Sound Blaster X4 Windows ARM64

Updated: 2026-09-03 KST

## Source of truth

Repository: `npark2860-cyber/SoundBlaster-X4-ARM64`

Default branch: `main`

At startup, verify actual GitHub branch heads and read in this order:

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260903_WINDOWS_CTCDC_PATH.md`
3. `DEBUG_HISTORY_20260903_CTCDC_NATIVE_UNLOCK_TRACE.md`
4. `DEBUG_HISTORY_20260903_CTCDC_MAX_PAYLOAD_RUNTIME.md`
5. `DEBUG_HISTORY_20260903_CTCDC_OPEN_SESSION_RUNTIME.md`
6. `DEBUG_HISTORY_20260903_DIRECT_MODE_RUNTIME_SUCCESS.md`
7. `NEXT_ACTION_CTCDC.md`

Do not reconstruct state from old chat context when GitHub is available.

## Project goal

Build an independent native Windows ARM64 controller for the locally USB-connected Creative Sound Blaster X4 / SB1815 on Snapdragon Windows ARM64.

Do not reintroduce Bluetooth as the Windows transport. Android BLE was protocol-discovery evidence only.

Device:

- Sound Blaster X4
- codename `Accent2`
- model/package `SB1815`
- USB VID/PID `041E:3278`

## Current milestone — Direct Mode works on Windows

Protocol/transport discovery for Direct Mode is complete for the currently observed X4 runtime state.

Hardware-confirmed path:

`COM3 open/configure`
→ CTCDC serial init
→ `5A 03 00` GetMaximumPayloadSize
→ RX `5A 03 02 3B 00`
→ Maximum Payload Size = 59
→ skip Unlock
→ skip `SW_MODE1`
→ `5A 09 01 02` GetFirmwareVersionString
→ firmware `1.7.250324.0910`
→ `5A 26 01 05` QueryButtonsAvailable
→ raw MIDAS write
→ `5A 39 03 00 05 01`
→ **physical X4 Direct Mode ON confirmed by the user**

This proves that the reconstructed CTCDC session path plus the exact six-byte Direct Mode command is sufficient for real Windows control on the tested X4 state.

No Direct Mode response bytes are claimed from the success run because its probe log was not uploaded with the physical confirmation.

## Fixed Direct Mode frames

- OFF: `5A 39 03 00 05 00`
- ON: `5A 39 03 00 05 01`

Construction:

- command `0x39` = FeatureControl
- SET operation = `0`
- Direct Mode bit position = `5`
- value = `0/1`
- header = `0x5A`

Do not revive obsolete `6A` or guessed `5C` variants.

## Windows USB topology

Composite device:

`USB\VID_041E&PID_3278`

Relevant interfaces:

- MI_00 / IF0: HID Consumer Control
- MI_01 / IF1+IF2: CDC ACM + CDC data, exposed as COM3 on the tested machine
- MI_03 / IF3: USB Audio 2.0 AudioControl
- IF4–IF6: USB AudioStreaming

CDC endpoints:

- Bulk OUT `0x03`
- Bulk IN `0x82`

Descriptor facts:

- total configuration length `1143`
- seven interfaces
- no vendor-class `0xFF` interface
- no UAC Extension Unit

## Creative Windows control chain

Managed trace:

`DirectModeFeatureViewModel`
→ `ToggleFeatureViewModel`
→ `IToggleFeature`
→ `Creative.Platform.Devices`
→ `CDCConnection.RawSetValue()`
→ `CDCConnection.Write()`
→ `ICTCDC.ExecuteCommand(1001, ...)`

`1001` = `CTCDCCMD_WritePassthroughData`.

Native static analysis confirms command 1001 writes the supplied raw bytes without adding another MIDAS wrapper.

Known COM identities:

- CCTCDC CLSID `{66FC4CF0-56C8-4523-A92B-CE69FCD7556A}`
- ICTCDC IID `{669E9C0E-AD66-48C3-8228-29A55C2E9977}`

## Native CTCDC fast path — hardware confirmed

Serial setup:

- event mask `0x05`
- 115200 baud
- 8N1
- DTR/RTS control bits disabled while preserving unrelated DCB flags
- zero COM timeouts
- `PurgeComm(0x0F)`
- `SETDTR`

Readiness query:

`5A 03 00`

Hardware RX:

`5A 03 02 3B 00`

Therefore the currently observed state follows the native `Open()` fast path and skips Unlock / `SW_MODE1`.

Required firmware query:

`5A 09 01 02`

Hardware RX:

`5A 09 12 02 10 31 2E 37 2E 32 35 30 33 32 34 2E 30 39 31 30 00`

Observed firmware string:

`1.7.250324.0910`

Buttons query:

`5A 26 01 05`

Hardware RX:

`5A 26 06 05 00 01 00 1E 00`

Do not assign undocumented meaning to the button payload without further evidence.

## Unlock path — contingency only

Exact supplied native reference binaries:

`CTCDC.dll`

- size `2,122,200`
- SHA-256 `bc4010e8f7000bfe6217425a0622dd710a7626d90fb61008505337aa87a43dab`
- PE32/x86

`CTIntrfu.dll`

- size `109,656`
- SHA-256 `ecf098101a0663568f4a406d7bed9775565a67213930e2487c17d858a5d0d9b6`
- PE32/x86

The fallback unlock algorithm is fully recovered but is not required while the initial max-payload query succeeds.

Fallback greeting:

`whoareyou.MyApp8\r\n`

Normal challenge:

- `whoareyou`
- seed4
- challenge32

Effective AES-256 key:

`seed[0:2] || D3 1A 21 27 9B E3 46 F0 99 9D 6E C4 C3 FE BE 98 90 18 69 C1 18 FB B1 25 6E 0C E0 7B || seed[2:4]`

Response:

`"unlock" || random16 || ciphertext32 || tag16 || "\r\n"`

Cipher:

- AES-256-GCM
- first 12 random bytes = nonce
- no AAD observed
- 16-byte tag

Expected success:

`unlock_OK\r\n`

Then:

`SW_MODE1\r\n`
→ `5A 03 00`
→ firmware query
→ buttons query

Do not force this path when fast-path readiness already succeeds.

## Eliminated paths — do not repeat

- Windows BLE control
- HID output/prefix guessing
- naked Direct Mode COM writes without CTCDC session setup
- UAC Extension Unit search
- vendor-class interface search
- `6A` Direct Mode variants
- guessed `5C` wrappers

## Current next task

The next phase is **productization**, not more protocol discovery.

Implement the first native Windows ARM64 controller with a deliberately narrow scope:

1. X4 CDC device/COM discovery
2. CTCDC-compatible serial open/init
3. fast-path session validation
4. Direct Mode ON
5. Direct Mode OFF
6. clean close/release

Do not add unrelated Creative features in this first implementation step.

See `NEXT_ACTION_CTCDC.md` for the exact scope.

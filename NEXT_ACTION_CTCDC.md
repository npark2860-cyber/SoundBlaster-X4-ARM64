# NEXT ACTION — Native Windows ARM64 Controller Implementation

Updated: 2026-09-04 KST

## Priority status

CTCDC work is **paused, not superseded**.

The current project priority is to finish the independent ASIO driver to a practical first-release capability level. Do not rediscover CTCDC transport/protocol work while ASIO B5 productization is active.

When ASIO first-release capability work is closed, resume from the already-proven CTCDC state below.

## Current status

Direct Mode protocol/transport discovery is **complete for the currently observed Sound Blaster X4 runtime state**.

Hardware-confirmed Windows path:

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
→ **physical X4 Direct Mode ON confirmed**

Runtime records:

- `DEBUG_HISTORY_20260903_CTCDC_MAX_PAYLOAD_RUNTIME.md`
- `DEBUG_HISTORY_20260903_CTCDC_OPEN_SESSION_RUNTIME.md`
- `DEBUG_HISTORY_20260903_DIRECT_MODE_RUNTIME_SUCCESS.md`

Full native reference trace:

- `DEBUG_HISTORY_20260903_CTCDC_NATIVE_UNLOCK_TRACE.md`

## Fixed Direct Mode frames

- ON: `5A 39 03 00 05 01`
- OFF: `5A 39 03 00 05 00`

Do not revive obsolete `6A`, guessed `5C`, HID-prefix, BLE, vendor-interface, or UAC Extension Unit paths.

## Next engineering phase when CTCDC resumes

Build the actual independent Windows ARM64 controller rather than another protocol probe.

Minimum first implementation scope:

1. locate the X4 CDC interface `USB\VID_041E&PID_3278&MI_01`
2. resolve its COM port
3. exclusive `CreateFileW` open
4. reproduce CTCDC serial setup:
   - event mask `0x05`
   - 115200 / 8N1
   - DTR/RTS control bits disabled while preserving unrelated DCB flags
   - zero COM timeouts
   - `PurgeComm(0x0F)`
   - `SETDTR`
5. establish the known fast-path session:
   - `5A 03 00`
   - require valid Maximum Payload Size
   - `5A 09 01 02`
   - require firmware response
   - `5A 26 01 05`
6. expose Direct Mode ON/OFF using the exact six-byte frames
7. close cleanly and release COM3

Do not add unrelated Creative features in this first productization step.

## Contingency unlock support

The native AES-256-GCM unlock algorithm is already recovered but is **not required in the currently observed state** because the initial Maximum Payload Size query succeeds.

Implement it only as a fallback path when a real runtime state fails the initial `5A 03 00` readiness query.

Recovered fallback sequence:

`whoareyou.MyApp8\r\n`
→ parse `whoareyou + seed4 + challenge32`
→ exact AES-256-GCM unlock response
→ require `unlock_OK\r\n`
→ `SW_MODE1\r\n`
→ retry `5A 03 00`
→ firmware query
→ buttons query

Do not force this path when the fast path already works.

## Validation rule

For every state-changing Creative command added after Direct Mode, require physical X4 confirmation. A successful Windows `WriteFile` alone is not sufficient validation.

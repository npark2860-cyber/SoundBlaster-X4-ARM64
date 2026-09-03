# DEBUG HISTORY — Direct Mode Runtime Success (2026-09-03)

## Scope

This document records the first hardware-confirmed successful Windows CTCDC Direct Mode control of the physical Sound Blaster X4 / SB1815.

Device path:

- local USB-connected Sound Blaster X4
- CDC interface `USB\VID_041E&PID_3278&MI_01`
- exposed as COM3 on the tested machine

## Preconditions already hardware-confirmed

The CTCDC fast-path session was validated on the same X4 runtime state:

1. `5A 03 00` — GetMaximumPayloadSize
   - RX `5A 03 02 3B 00`
   - Maximum Payload Size = 59 bytes
2. `5A 09 01 02` — GetFirmwareVersionString
   - RX `5A 09 12 02 10 31 2E 37 2E 32 35 30 33 32 34 2E 30 39 31 30 00`
   - firmware string observed: `1.7.250324.0910`
3. `5A 26 01 05` — QueryButtonsAvailable
   - RX `5A 26 06 05 00 01 00 1E 00`

Because the initial maximum-payload query succeeded, the native CTCDC `Open()` fast path skips Unlock and `SW_MODE1` in this runtime state.

## Direct Mode command

The one-shot test then sent exactly one state-changing MIDAS frame:

`5A 39 03 00 05 01`

Meaning:

- command `0x39` = FeatureControl
- SET operation = `0`
- Direct Mode feature bit position = `5`
- value = `1`

## Hardware result

The user confirmed that the test **works**: the physical X4 successfully entered Direct Mode after the CTCDC session-validation sequence followed by the exact six-byte ON frame.

This is the first hardware confirmation on Windows that the recovered CTCDC transport/session path plus the known MIDAS Direct Mode command is sufficient for actual X4 control.

No new wrapper, HID report prefix, Bluetooth path, UAC Extension Unit, or vendor-class USB interface is required for this tested state.

A direct-mode probe log was not uploaded with this confirmation, so this document does not invent or claim any Direct Mode response bytes beyond the physical state-change confirmation.

## Confirmed control path

`COM3 open/configure`
→ CTCDC serial init
→ `5A 03 00`
→ valid max-payload response
→ skip Unlock
→ skip `SW_MODE1`
→ `5A 09 01 02`
→ valid firmware response
→ `5A 26 01 05`
→ capability response
→ raw MIDAS write
→ `5A 39 03 00 05 01`
→ **physical X4 Direct Mode ON confirmed**

## Fixed Direct Mode frames

- ON: `5A 39 03 00 05 01`
- OFF: `5A 39 03 00 05 00`

The OFF frame remains protocol-confirmed from the same managed/native construction path; the key milestone recorded here is the Windows hardware confirmation of ON.

## Consequence

Protocol/transport discovery for Direct Mode is complete for the currently observed X4 runtime state.

The next engineering phase is no longer protocol guessing. It is productization of the independently reconstructed Windows ARM64 controller:

- reusable CTCDC session layer
- X4 device/COM discovery
- safe open/close lifecycle
- Direct Mode ON/OFF API
- later expansion to additional already-recovered Creative controls only after each command is independently validated

The recovered AES-256-GCM unlock path remains a contingency implementation for future runtime states where `5A 03 00` does not initially succeed.

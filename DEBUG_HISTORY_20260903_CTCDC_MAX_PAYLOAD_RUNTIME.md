# DEBUG HISTORY — CTCDC Maximum Payload Runtime (2026-09-03)

## Scope

Hardware runtime result from the Windows ARM64 CTCDC serial probe on the user's Sound Blaster X4 / SB1815.

This document records only observed runtime facts and their interpretation against the previously recovered `CTCDC.dll` control flow.

## Runtime log

The probe successfully detected and opened:

`COM3`

Observed original DCB:

- baud: `115200`
- byte size: `8`
- parity: `0`
- stop bits: `0`

The probe applied the CTCDC serial initialization and transmitted:

`5A 03 00`

The physical X4 replied with exactly five bytes:

`5A 03 02 3B 00`

Decoded using the binary-confirmed `DoExecuteCommand_CTCDCCMD_GetMaximumPayloadSize` parser:

- framing: `5A`
- command: `03`
- payload length: `02`
- payload: `3B 00`
- Maximum Payload Size: `0x003B` = `59` bytes

## Consequence for the CTCDC state machine

This is the successful first branch of `ICTCDC::Open` recovered from `CTCDC.dll`.

Therefore, in the tested X4 state, CTCDC does **not** enter the automatic unlock path and does **not** execute `SW_MODE1` before continuing.

Observed/reconstructed runtime path is now:

`COM open/configure`
→ `5A 03 00` GetMaximumPayloadSize
→ response `5A 03 02 3B 00`
→ Maximum Payload Size = 59
→ skip Unlock
→ skip SetSwMode1
→ next required CTCDC Open step: `5A 09 01 02` GetFirmwareVersionString
→ then `5A 26 01 05` QueryButtonsAvailable

## Probe artifact note

The uploaded log still printed the obsolete labels:

- `TX firmware query`
- `VALID firmware response found`

Those strings identify the executable as an older build artifact from before the static-analysis correction. This does not invalidate the raw runtime bytes above. The bytes `5A 03 02 3B 00` are interpreted according to the exact current CTCDC native trace, where command `0x03` is Maximum Payload Size.

## Evidence classification

### Hardware-confirmed

- COM3 opens successfully after the previous access-denied issue was removed.
- CTCDC-style serial initialization succeeds.
- X4 responds to `5A 03 00` with `5A 03 02 3B 00`.
- Maximum Payload Size is 59 bytes in this runtime state.

### Binary-confirmed interpretation

- successful command-`0x03` response causes `ICTCDC::Open` to skip Unlock and `SetSwMode1`.
- next required `Open()` command is `5A 09 01 02`.
- `5A 26 01 05` follows as the button-capability query.

### Not yet hardware-confirmed

- response to `5A 09 01 02`
- response to `5A 26 01 05`
- Direct Mode passthrough after this session path

## Next action

Extend the safe ARM64 probe only through the remaining non-state-changing CTCDC `Open()` queries:

1. require valid `5A 03 00` Maximum Payload Size response
2. send `5A 09 01 02` and capture exact response
3. send `5A 26 01 05` and capture exact response
4. stop without sending Direct Mode

Do not implement or send the unlock response on this already-responsive runtime path.

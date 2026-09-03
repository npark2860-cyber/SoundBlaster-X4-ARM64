# DEBUG HISTORY — CTCDC Open-session Runtime Validation (2026-09-03)

## Scope

Hardware runtime validation on the user's locally USB-connected Sound Blaster X4 (`041E:3278`) using COM3 and the CTCDC-equivalent serial initialization previously recovered from `CTCDC.dll`.

This document records only hardware-observed runtime facts from the Stage C2 probe.

## Runtime result

Serial initialization succeeded with:

- communication mask `0x05`
- `115200/8N1`
- DCB mask behavior equivalent to `0xFFFFCFCF`
- zero COM timeouts
- `PurgeComm(0x0F)`
- `SETDTR`

### 1. Maximum payload size

TX:

`5A 03 00`

RX:

`5A 03 02 3B 00`

Parsed result:

- command `0x03`
- payload length `2`
- Maximum Payload Size = `0x003B` = **59 bytes**

This confirms the initial CTCDC readiness query succeeds in the current X4 runtime state.

Therefore, matching the binary-confirmed `ICTCDC::Open()` control flow, this state takes the fast path and does **not** require Unlock or `SW_MODE1` before continuing.

### 2. Firmware version string

TX:

`5A 09 01 02`

RX:

`5A 09 12 02 10 31 2E 37 2E 32 35 30 33 32 34 2E 30 39 31 30 00`

ASCII payload includes:

`1.7.250324.0910`

This hardware response proves `GetFirmwareVersionString` is responsive on the reconstructed CTCDC session path.

### 3. Button capabilities query

TX:

`5A 26 01 05`

RX:

`5A 26 06 05 00 01 00 1E 00`

The response is non-empty and command-matched. Exact semantic decoding of the capability payload is not required for the current Direct Mode objective and is intentionally not guessed here.

## Session conclusion

The current hardware-confirmed fast path is:

`serial init`
→ `5A 03 00`
→ RX `5A 03 02 3B 00`
→ skip Unlock
→ skip `SW_MODE1`
→ `5A 09 01 02`
→ valid firmware response containing `1.7.250324.0910`
→ `5A 26 01 05`
→ valid command-matched response
→ CTCDC Open-session validation complete

No unlock response, `SW_MODE1`, Direct Mode frame, or unknown command was sent during this Stage C2 runtime test.

## Next controlled step

Proceed to a one-command-at-a-time Direct Mode passthrough test using the already fixed raw frames:

- ON: `5A 39 03 00 05 01`
- OFF: `5A 39 03 00 05 00`

Each run must:

1. reproduce the same CTCDC serial setup
2. verify the initial `5A 03 00` max-payload response before any state-changing command
3. send only one Direct Mode frame
4. record any RX bytes
5. require physical X4 confirmation; successful `WriteFile` alone is not validation

Do not reintroduce BLE, HID guessing, vendor-interface searches, UAC Extension Unit searches, `6A`, or guessed `5C` frames.

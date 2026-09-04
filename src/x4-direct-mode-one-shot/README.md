# Sound Blaster X4 CTCDC Direct Mode One-Shot

This is a narrow reproduction of the Windows CTCDC Direct Mode sequence that was hardware-confirmed on 2026-09-03.

It deliberately reuses the exact previously validated `src/serial-poc/session-open-probe.cpp` implementation in the same translation unit. The CTCDC serial setup/query implementation is not rewritten.

## Sequence

1. Auto-detect `USB\\VID_041E&PID_3278&MI_01` and its current COM port.
2. Apply the validated CTCDC serial initialization.
3. Send `5A 03 00` and require a valid command `0x03` maximum-payload response.
4. Send `5A 09 01 02` and require a valid firmware response.
5. Send `5A 26 01 05` and require a valid buttons response.
6. Only if all three checks pass, send exactly one state-changing frame:
   - ON: `5A 39 03 00 05 01`
   - OFF: `5A 39 03 00 05 00`
7. Close the serial session.

If any session-validation response is missing or malformed, the program stops and does not send Direct Mode ON/OFF.

It does not send an unlock response or `SW_MODE1`.

## Easy run

Double-click one file:

- `RUN-DIRECT-MODE-ON.cmd`
- `RUN-DIRECT-MODE-OFF.cmd`

No COM-port argument is normally required.

The diagnostic log is written as:

`x4-ctcdc-probe.txt`

Physical X4 state change is the validation criterion. A successful `WriteFile` alone is not treated as hardware validation.

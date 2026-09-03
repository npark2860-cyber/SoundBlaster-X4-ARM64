# Sound Blaster X4 Windows ARM64 CTCDC Serial Probe

Read/diagnostic probe for the X4's `USB\\VID_041E&PID_3278&MI_01` CDC serial interface.

## Current hardware-confirmed state

The physical X4 has replied to:

```text
5A 03 00
```

with:

```text
5A 03 02 3B 00
```

Static analysis of the exact `CTCDC.dll` identifies command `0x03` as `CTCDCCMD_GetMaximumPayloadSize`, so the observed maximum payload size is:

```text
0x003B = 59 bytes
```

Because this query succeeds, the recovered `ICTCDC::Open()` path skips Unlock and `SW_MODE1` in this runtime state.

## What this build tests

The probe reproduces CTCDC serial initialization:

- communication mask `0x05` (`EV_RXCHAR | EV_TXEMPTY`)
- `115200` baud
- `8` data bits
- no parity
- one stop bit
- DTR/RTS control disabled while unrelated DCB flags are preserved
- zero COM timeouts
- `PurgeComm(0x0F)`
- `EscapeCommFunction(SETDTR)`

It then follows only the already-responsive CTCDC `Open()` branch:

1. `5A 03 00` — `GetMaximumPayloadSize`
2. `5A 09 01 02` — `GetFirmwareVersionString`
3. `5A 26 01 05` — `QueryButtonsAvailable`
4. stop

If the first maximum-payload query unexpectedly fails, this build stops. It does not attempt unlock.

## Run

```powershell
.\\x4-serial-ctcdc-probe.exe
```

The X4 COM port is auto-detected. An explicit port can be supplied:

```powershell
.\\x4-serial-ctcdc-probe.exe COM3
```

Output is written to:

```text
x4-ctcdc-probe.txt
```

Upload that text file for analysis.

## Safety / scope

This build does **not** send:

- an unlock response
- `SW_MODE1`
- Direct Mode ON/OFF
- any other state-changing command

It only validates the remaining read/query portion of CTCDC's successful `Open()` path before passthrough is tested.

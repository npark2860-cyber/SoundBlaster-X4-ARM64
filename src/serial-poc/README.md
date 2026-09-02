# Sound Blaster X4 Windows ARM64 CTCDC Serial Probe

Read/diagnostic probe for the X4's `USB\\VID_041E&PID_3278&MI_01` CDC serial interface.

This version exists because reverse engineering of Creative's `CTCDC.dll` showed that the previous raw serial PoC did **not** reproduce Creative's serial initialization. CTCDC configures the COM port before sending any protocol frame.

## CTCDC initialization reproduced

The probe applies:

- communication mask `0x05` (`EV_RXCHAR | EV_TXEMPTY`)
- `115200` baud
- `8` data bits
- no parity
- one stop bit
- zero COM timeouts
- `PurgeComm(0x0F)`
- `EscapeCommFunction(SETDTR)`

It then sends CTCDC's first normal protocol probe:

```text
5A 03 00
```

This is the firmware query used by CTCDC before it decides whether an unlock is needed.

If no valid command-`0x03` response is received, the tool follows CTCDC only one harmless step further and sends its unlock greeting:

```text
whoareyou.MyApp8\r\n
```

It records the response but deliberately stops before generating or sending the cryptographic unlock reply.

## Run

```powershell
.\\x4-serial-ctcdc-probe.exe
```

The X4 COM port is auto-detected. An explicit port can be supplied:

```powershell
.\\x4-serial-ctcdc-probe.exe COM3
```

Output is written to both the console and:

```text
x4-ctcdc-probe.txt
```

Upload that text file for analysis.

## Safety / scope

This probe does **not** send Direct Mode ON/OFF. It performs only the serial initialization, a firmware query, and when needed the first unlock greeting observed in Creative's own CTCDC implementation.

# Sound Blaster X4 Windows ARM64 CTCDC Serial Probe

Read/diagnostic probe for the X4's `USB\\VID_041E&PID_3278&MI_01` CDC serial interface.

This version exists because reverse engineering of Creative's exact `CTCDC.dll` showed that the previous raw serial PoC did **not** reproduce Creative's serial/session initialization.

## CTCDC initialization reproduced

The probe applies:

- communication mask `0x05` (`EV_RXCHAR | EV_TXEMPTY`)
- `115200` baud
- `8` data bits
- no parity
- one stop bit
- DTR/RTS control disabled while unrelated DCB flags are preserved
- zero COM timeouts
- `PurgeComm(0x0F)`
- `EscapeCommFunction(SETDTR)`

It then sends CTCDC's first session-readiness query:

```text
5A 03 00
```

Static analysis confirms this is `CTCDCCMD_GetMaximumPayloadSize`. A valid command-`0x03`, 2-byte response returns the maximum payload size; it is **not** a firmware-version query.

If no valid maximum-payload response is received, the tool follows CTCDC only one harmless step further and sends its unlock greeting:

```text
whoareyou.MyApp8\r\n
```

It records the response but deliberately stops before generating or sending the cryptographic unlock reply.

Static analysis has recovered the later CTCDC path (`AES-256-GCM` unlock response, `SW_MODE1`, post-unlock maximum-payload retry, firmware-version query, and passthrough), but this probe remains intentionally limited until the first real X4 challenge is captured.

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

This probe does **not** send an unlock reply, `SW_MODE1`, or Direct Mode ON/OFF. It performs only the CTCDC serial initialization, the maximum-payload readiness query, and when needed the first unlock greeting observed in Creative's own CTCDC implementation.

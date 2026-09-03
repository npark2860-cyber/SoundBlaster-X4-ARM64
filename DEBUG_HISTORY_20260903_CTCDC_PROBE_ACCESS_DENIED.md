# DEBUG HISTORY — CTCDC Probe COM3 Access Denied (2026-09-03)

## Runtime source

User-provided `x4-ctcdc-probe.txt` from the ARM64 hardware run.

Observed output:

```text
Sound Blaster X4 CTCDC serial initialization probe
Read/diagnostic only after harmless protocol queries; no Direct Mode change is sent.
Port: COM3
CreateFileW failed: 5
```

## Confirmed facts

- The probe successfully found the Sound Blaster X4 `MI_01` CDC serial interface and resolved it to `COM3`.
- The failure occurred before any DCB configuration, `5A 03 00` query, unlock greeting, or other protocol traffic.
- Win32 error `5` is `ERROR_ACCESS_DENIED`.
- The current probe opens `\\.\COM3` with `GENERIC_READ | GENERIC_WRITE`, share mode `0`, `OPEN_EXISTING`.
- Therefore this run provides no evidence yet about the X4 response to `GetMaximumPayloadSize`, unlock, `SW_MODE1`, or passthrough.

## Immediate interpretation

The next issue to resolve is access to the COM3 device handle, not CTCDC protocol framing.

Two relevant causes remain at this point:

1. another process already has COM3 opened in an incompatible/exclusive mode;
2. local access policy/permissions deny the probe's requested handle.

Do not reinterpret this result as a protocol failure.

## Next action

Before changing CTCDC protocol code, ensure Creative App and any Creative process that may own the CDC session are fully closed, then rerun the same safe probe.

If `CreateFileW` still returns `ERROR_ACCESS_DENIED`, add a focused Windows handle-owner/access diagnostic rather than changing protocol bytes.

## Scope discipline

Do not send unlock replies, `SW_MODE1`, Direct Mode, HID, BLE, or naked alternate serial frames until COM3 can be opened successfully.

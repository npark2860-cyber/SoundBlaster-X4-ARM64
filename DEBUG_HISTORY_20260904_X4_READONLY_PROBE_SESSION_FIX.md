# X4 Read-Only Capability Probe — First Runtime Fix

Updated: 2026-09-04 KST

## Runtime result

First hardware run of `x4-control-readonly-probe.exe`:

- X4 MI_01 resolved as `COM3`.
- CTCDC-like serial initialization completed.
- TX `5A 03 00` GetMaximumPayloadSize.
- RX: 0 bytes.
- Probe stopped before all capability queries.
- Completed read-only queries: `0 / 83`.

No state-changing command was sent.

## Comparison with previously validated CTCDC session probe

The new generic transaction helpers performed an additional `PurgeComm(handle, PURGE_RXCLEAR)` immediately before every query. The previously hardware-validated session-open path did not perform this per-query RX purge; it only performed the CTCDC initialization purge before `SETDTR` and then wrote the query.

## Single-variable correction

Commit `a8e0d63e8a5641b5ccfc642e44919494b300fc29` removes only the per-query `PURGE_RXCLEAR` calls from `transact_capture()` and `transact()`.

Unchanged:

- exclusive COM open;
- `115200/8N1`;
- event mask `0x05`;
- zero timeouts;
- initial `PurgeComm(0x0F)`;
- `SETDTR`;
- 100 ms post-init settle;
- exact `5A 03 00` first query;
- all read-only capability query definitions;
- no SET/raw-command interface.

## Next validation

Rebuild `Build X4 Read-Only Capability Probe ARM64` on branch `exp/windows-arm64-x4-readonly-capability-map`, execute once, and inspect `X4_READONLY_CAPABILITY_REPORT.txt`.

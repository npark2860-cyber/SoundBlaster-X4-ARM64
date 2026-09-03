# ASIO Stage A1 — lifecycle isolation implemented

Updated: 2026-09-03 KST

## Why this stage exists

Native ARM64 Stage A0 passed on real Sound Blaster X4 hardware:

- one complete open/run/stop/close lifecycle
- 20/20 WaveRT notifications
- packet continuity clean
- presentation position monotonic
- clean unregister and handle close

Therefore native ARM64 ABI, `KsCreatePin`, WaveRT buffer allocation, event registration, a single KS state lifecycle, and cleanup are hardware-confirmed.

The earlier Stage A build that caused `WDF_VIOLATION (0x10D)` differed from A0 in two material ways:

1. repeated reopen/lifecycle execution
2. writes into the WaveRT half-buffer during RUN

Stage A1 isolates only item 1.

## Only changed variable from A0

Repeat the complete proven A0 lifecycle three times in the same process.

Each lifecycle remains exactly:

1. discover X4 `msft_wave`
2. open filter
3. create Render Pin 1
4. 48 kHz / stereo / 16-bit PCM
5. request 4096-byte `BUFFER_WITH_NOTIFICATION`, notification count 2
6. zero entire buffer once before RUN
7. register notification event
8. `ACQUIRE -> PAUSE -> RUN`
9. observe exactly 20 notifications
10. query `PACKETCOUNT` and `PRESENTATION_POSITION`
11. **no DMA buffer writes during RUN**
12. `PAUSE -> ACQUIRE -> STOP`
13. unregister notification event
14. close event/pin/filter

Then repeat from step 1, for three lifecycles total.

## Success criteria

- three clean lifecycles
- 60 total notifications
- `packet_discontinuities=0`
- `position_regressions=0`
- no bugcheck

Expected summary:

```text
notifications=60
packet_discontinuities=0
position_regressions=0
STAGE A1 LIFECYCLE RESULT: PASS
```

## Interpretation

If Stage A1 passes, repeated open/run/stop/close is exonerated and the old Stage A crash points strongly to the RUN-time DMA half-buffer write behavior.

If Stage A1 bugchecks, do not retry. Use the checkpoint log and new minidump to determine which lifecycle transition or cleanup/reopen step triggers the WDF handle-type violation.

## Safety constraint

The old crashing Stage A binary remains quarantined. Do not run it again.

# NEXT ACTION — Native ARM64 / ARM64EC ASIO

Updated: 2026-09-04 KST

## Validated baseline

B4D remains the proven fallback:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Known-good B4D runtime:

- REAPER ARM64EC real playback
- 48 kHz
- stereo output
- signed 16-bit PCM
- 512 ASIO frames
- Render Pin 1
- local + global BUSY gates
- joined worker stop
- ASIO 2.x time-info

Do not alter B4D unless B5 exposes a concrete regression.

---

# Current B5 source

`exp/windows-arm64-asio-b5-capability-productization@c69cfa98a497c0619ccdbe0fb7f40f0dd13ea687`

B5 first-release contract remains:

- 2 output channels, Int24LSB
- 2 input channels at 48/96 kHz, Int24LSB
- output 48/96/192 kHz
- 192 kHz reports zero input channels
- buffer 96..4800 / granularity 48 / preferred 240
- 512 compatibility exception
- Internal Clock
- ASIO 2.x time-info
- Render Pin 1 + Capture Pin 4 WaveRT
- immutable local/global BUSY gates before pin creation
- joined worker before hardware teardown

---

# Latest runtime evidence

Returned report generated 2026-09-04 11:50:11.

### PASS — 48 kHz / 240 output-only

Three cycles:

- callbacks 139 / 139 / 140
- stop=ASE_OK
- workerJoined=YES
- no packet/index/copy errors

### PASS — 48 kHz / 240 full duplex

Two cycles:

- callbacks=141 each
- renderNotif=142 each
- captureNotif=141 each
- outFrames=inFrames=33840 each
- stop=ASE_OK

`inputNonzeroSamples=0` still means actual microphone/line signal content remains for the final REAPER test.

### FAIL — 96 kHz / 240 full duplex

First cycle ended after 97 callbacks with:

- worker=1
- idx=8
- outCopy=0
- inCopy=0
- rPkt=9
- rPos=0
- cPkt=1

The trace shows repeated render packet jumps (`23->25`, `35->37`, `43->45`, etc.) and later a capture packet jump plus `GETREADPACKET` Win32 21.

The defect is now identified as the original full-duplex worker's serial wait structure. It waited render, then capture, on one thread. At a 2.5 ms period, auto-reset render notifications could coalesce while the thread was blocked on capture.

See:

`DEBUG_HISTORY_20260904_ASIO_B5_96K_DUPLEX_EVENT_COALESCING_MUX_FIX.md`

---

# Fix now implemented — dual-event-mux-v1

New shared B5 worker adapter:

`src/asio-arm64-stage-b0/driver_b5_mux_adapter.inl`

Both ARM64EC and Classic ARM64 B5 builds route worker creation through it.

Full-duplex worker simultaneously waits on:

1. stop
2. capture notification
3. render notification

Capture is intentionally the lower wait index when both direction events are already signaled.

Behavior:

- render/capture events serviced independently
- two capture slots tagged with absolute capture packet numbers
- render packet N paired with exact capture packet N-1
- ASIO callback runs only when that exact pair exists
- render write-ahead remains `renderPacket + 1`
- second render arrival before previous pair synchronization is a real failure
- capture `ERROR_NOT_READY` is transient/no-data, not immediate hardware failure
- `MoreData=TRUE` is drained immediately

Strict failures remain:

- render packet discontinuity
- capture packet discontinuity
- render presentation-position regression
- repeated callback buffer index
- render/capture copy failure
- duplex synchronization failure

Realtime worker itself enters MMCSS `Pro Audio` with `AVRT_PRIORITY_CRITICAL`; fallback is `THREAD_PRIORITY_HIGHEST` only if MMCSS registration fails.

The old per-notification printf path is bypassed in the new worker hot path.

---

# Mandatory runtime/build marker

Marker:

`dual-event-mux-v1`

Expected new runtime lines:

`B5 worker realtime adapter=dual-event-mux-v1 ...`

`B5 worker START adapter=dual-event-mux-v1 ...`

The main build workflow now scans both produced DLL binaries for this marker. Packaging fails if either ARM64EC or Classic DLL lacks it.

Therefore do not accept a runtime report as a test of this fix unless the report contains `adapter=dual-event-mux-v1`.

---

# Immediate action

Do **not** reuse any previous validation ZIP.

Run manual workflow:

`Build ASIO B5 Productization`

Required build outcome:

1. checkout current B5 branch;
2. ARM64EC compile/link PASS;
3. Classic ARM64 compile/link PASS;
4. PE/ARM64X checks PASS;
5. log contains `B5 mux runtime marker verified in both DLLs`;
6. only then package/upload ZIP.

If build fails, fix on the same B5 branch. Do not hardware-test a partial package.

After Actions PASS:

1. download the new `SoundBlaster-X4-ASIO-B5-Productization.zip`;
2. close REAPER/media/Creative App and other X4 playback; move Windows default endpoint away if needed;
3. run the new `install_and_validate_b5.cmd` once;
4. return `B5_PRODUCT_VALIDATION_REPORT.txt`.

The next report must contain `adapter=dual-event-mux-v1` and must attempt the full strict matrix:

- 48k/240 output x3
- 48k/240 full duplex x2
- 96k/240 full duplex x2
- 192k/240 output x2
- 48k/96 output x1
- 48k/4800 output x1
- 48k/512 compatibility output x1

Only after full matrix PASS should final REAPER validation cover audible 24-bit output plus real stereo input together.

## Immutable safety

Never bypass BUSY.

Historical collision class remains:

- `WDF_VIOLATION 0x10D`
- Parameter 1 = 5
- stale/destroyed `WDFUSBPIPE` path in `usbaudio2` recovery

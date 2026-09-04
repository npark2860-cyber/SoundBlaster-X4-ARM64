# DEBUG HISTORY — ASIO B5 96 kHz duplex event coalescing / mux fix

Date: 2026-09-04 KST

## Returned runtime report

`B5_PRODUCT_VALIDATION_REPORT(2).txt` generated 2026-09-04 11:50:11.

The package again passed:

- B5 registration and registry verification
- immutable property-only Render Pin 1 idle gate `C 0/1 G 0/1`, BUSY=NO
- static KS capability probe
- B5 COM/public ASIO capability report
- 2 input / 2 output channels at 48/96 kHz capture scope
- Int24LSB type 17
- buffers 96..4800 / preferred 240 / granularity 48
- 48/96/192 kHz output scope
- Internal Clock
- latency 240/240 at preferred size
- ASIO time-info support

## Runtime progress retained

### 48 kHz / 240 output-only

Three cycles passed:

- callbacks 139 / 139 / 140
- stop=ASE_OK
- workerJoined=YES
- no packet/index/copy errors

### 48 kHz / 240 full duplex

Two cycles passed:

- cycle 1: callbacks=141, renderNotif=142, captureNotif=141, outFrames=33840, inFrames=33840
- cycle 2: callbacks=141, renderNotif=142, captureNotif=141, outFrames=33840, inFrames=33840
- both stop=ASE_OK

The silent validation still reports `inputNonzeroSamples=0`; this proves capture lifecycle/copy survival, not real external signal content.

## 96 kHz failure pattern

The first 96 kHz / 240-frame full-duplex cycle reached RUN but failed after 97 callbacks.

Final strict stop diagnostic:

- worker=1
- idx=8
- outCopy=0
- inCopy=0
- rPkt=9
- rPos=0
- cPkt=1

Immediately before exit:

- render packet sequence had repeatedly skipped absolute packets
- capture eventually skipped packet 96 and returned packet 97
- a later capture `GETREADPACKET` returned Win32 21 (`ERROR_NOT_READY`)

The trace shows the structural pattern directly. Examples include:

- render 23 -> 25
- render 35 -> 37
- render 43 -> 45
- later larger gaps as the worker fell further behind

Capture remained mostly sequential until the worker had already accumulated substantial render lag.

## Root cause

The original B5 full-duplex worker used one thread and serviced directions serially:

1. wait for one render notification;
2. query render PACKETCOUNT;
3. wait for one capture notification;
4. query GETREADPACKET;
5. invoke the ASIO callback and write the next render packet;
6. repeat.

The WaveRT notification events are auto-reset events. At 96 kHz / 240 frames, each period is 2.5 ms. While the worker is blocked waiting for capture, another render notification can arrive before the worker waits on the render event again. Auto-reset event state is not a counting semaphore, so multiple render notifications can collapse into one signaled state. The next PACKETCOUNT query then observes an absolute packet jump.

At 48 kHz / 240 frames the 5 ms period left enough margin for the serial wait pattern to survive. At 96 kHz it did not.

The previous MMCSS-only change therefore addressed scheduler priority but not this structural event-coalescing window.

The absence of the expected prior MMCSS runtime marker in the returned report also made it impossible to prove from the text alone that the newest adapter binary had actually been loaded. The new build workflow now rejects any B5 DLL that does not contain the new runtime marker.

## GETREADPACKET Win32 21 interpretation

Microsoft documents that `GetReadPacket` can return `STATUS_DEVICE_NOT_READY` when no new captured data is available. User-mode `DeviceIoControl` exposes that condition as Win32 `ERROR_NOT_READY` (21).

Therefore the new worker treats `ERROR_NOT_READY` as a transient no-data condition and counts it, rather than immediately classifying the hardware as failed.

This does **not** relax packet integrity. A capture packet-number discontinuity remains fatal.

Microsoft also documents that `MoreData=TRUE` permits another immediate `GetReadPacket` call. The new worker drains that condition without waiting for the next real-time period.

## Fix implemented

Current B5 branch after the fix:

`exp/windows-arm64-asio-b5-capability-productization@c69cfa98a497c0619ccdbe0fb7f40f0dd13ea687`

Added:

`src/asio-arm64-stage-b0/driver_b5_mux_adapter.inl`

ARM64EC and Classic ARM64 B5 adapters now route B5's worker thread through the same `dual-event-mux-v1` worker.

### Full duplex

The worker waits on three handles simultaneously:

1. stop event
2. capture notification event
3. render notification event

Capture intentionally has the lower event index so that, if both direction events are already signaled when the worker wakes, capture is serviced first rather than starved by continuously arriving render events.

The worker:

- processes render and capture notifications independently
- keeps two absolute capture-packet slot tags matching the WaveRT notification count
- pairs render packet N with capture packet N-1
- only invokes the ASIO callback when that exact capture packet is available
- preserves the existing write-ahead rule `writePacket = renderPacket + 1`
- treats a second render notification arriving before the prior render/capture pair can synchronize as a real duplex failure

### Capture semantics

- `ERROR_NOT_READY` -> transient, counted, not fatal by itself
- `MoreData=TRUE` -> immediate drain permitted
- capture packet-number discontinuity -> still fatal

### Render semantics

- PACKETCOUNT discontinuity -> still fatal
- presentation-position regression -> still fatal
- callback buffer-index repetition -> still fatal
- render/capture copy failure -> still fatal

### Realtime scheduling

The replacement worker itself now enters:

- MMCSS task `Pro Audio`
- `AVRT_PRIORITY_CRITICAL`
- fallback `THREAD_PRIORITY_HIGHEST` only if MMCSS registration fails
- `AvRevertMmThreadCharacteristics` before exit

The hot packet path no longer calls the original per-notification `printf` path. Runtime output is reduced to start/exit summaries and failures.

## Runtime marker / package verification

A new mandatory runtime marker is embedded:

`dual-event-mux-v1`

The main Actions workflow now verifies this ASCII marker exists in both built DLLs before packaging:

- ARM64EC B5 DLL
- Classic ARM64 B5 DLL

If either DLL lacks the marker, the workflow fails and no validation ZIP is produced.

Expected runtime lines from the new DLL include:

`B5 worker realtime adapter=dual-event-mux-v1 ...`

and

`B5 worker START adapter=dual-event-mux-v1 ...`

## Safety

The immutable BUSY policy remains unchanged.

Never bypass:

- Render Pin 1 local/global init preflight
- Render Pin 1 local/global re-check before `KsCreatePin`
- Capture Pin 4 local/global re-check before `KsCreatePin`
- joined worker before hardware teardown

Validated B4D remains untouched. Compare against B4D still has merge base exactly `a95a95d014bcc1c3a521be41325841ae96dc8a61`, behind=0.

## Next action

Run manual workflow:

`Build ASIO B5 Productization`

The workflow must build current B5 head and must report:

`B5 mux runtime marker verified in both DLLs`

Do not hardware-test if compile/link/marker verification fails.

After Actions PASS, use the new ZIP and run `install_and_validate_b5.cmd` once.

The returned report must contain `adapter=dual-event-mux-v1` before any runtime matrix result is accepted as a test of this fix.

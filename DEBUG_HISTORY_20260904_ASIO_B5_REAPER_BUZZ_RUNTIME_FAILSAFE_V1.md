# DEBUG HISTORY — ASIO B5 REAPER sustained buzz / runtime fail-safe v1

Date: 2026-09-04 KST

## Context

The silent B5 product matrix had already passed completely, including:

- 48k/240 output x3
- 48k/240 duplex x2
- 96k/240 duplex x2
- 192k/384 output x2
- 48k/96
- 48k/4800
- 48k/512 compatibility

Final silent result:

`B5 PRODUCT VALIDATION RESULT: PASS code=0`

`B5 INSTALL + PRODUCT VALIDATION: PASS`

## Real REAPER host evidence

REAPER ARM64EC showed the B5 device active at:

- 48 kHz
- 24-bit
- 2 inputs / 2 outputs
- 480 samples
- approximately 10 ms input + 10 ms output latency

480 frames at 48 kHz is exactly 10 ms and is within the B5 public buffer contract.

During actual audible playback, the output later changed into a very loud sustained `drone/buzz` tone. REAPER itself did not provide a useful log for this event.

This means the silent matrix did not fully cover a real-audio failure mode.

## Leading mechanism

The current mux-v3 fatal path previously did only:

1. set `worker_failed_`;
2. print a failure message;
3. return from the worker thread.

It did not immediately overwrite the WaveRT render cyclic contents and did not transition the render pin out of RUN inside the worker.

Therefore a plausible failure mode was:

`normal audio -> worker fatal exit -> WaveRT pin still RUN -> last cyclic audio contents repeat -> sustained buzz/drone`

This is a measured host symptom with a concrete matching code path, but the exact worker failure reason is not yet known because the previous build had no persistent failure record.

Do not claim the root cause is known until the new runtime record captures an actual failure.

## Implemented safety/diagnostic change

B5 branch:

`exp/windows-arm64-asio-b5-capability-productization@075010999bed5c433f03d7421f2fa3a18221bd98`

New marker:

`runtime-failsafe-v1`

Changed file:

`src/asio-arm64-stage-b0/driver_b5_mux_adapter.inl`

On every fatal worker path:

1. snapshot render/capture stats, engine messages, Win32 state, callback/index/copy counters, and capture phase counters;
2. set `worker_failed_`;
3. immediately overwrite both render notification slots with silence through the existing `write_render_packet24()` API;
4. do **not** perform `KSSTATE` transitions, pin close, dispose, or hardware teardown inside the worker;
5. only after the silence attempt, emit the diagnostic record to `OutputDebugString` and write:

`%TEMP%\B5_RUNTIME_FAILURE.txt`

The existing invariant remains unchanged:

**worker must be joined before hardware teardown.**

The emergency silence operation may alter post-failure render write counters, so the diagnostic stats are snapshotted before the two zero writes.

## Runtime record fields

The failure file records:

- marker / adapter
- local timestamp and tick count
- process/thread id
- direction and failure reason
- captured Win32 error value
- whether emergency silence succeeded
- sample rate and ASIO buffer frames
- render/capture selected and running state
- callback count and last callback index
- callback-index / render-copy / capture-copy errors
- render notification / packet-discontinuity / position-regression / write / frame / nonzero / last-packet stats
- capture equivalent stats
- captureNotReady / captureMoreData / capturePhaseMisses / captureConsumed
- render and capture engine messages

## REAPER-matched silent case added

The product validator now also includes:

`48 kHz / 480 frames / output-only / 5 seconds`

case name:

`reaper-48-480-output`

This does not reproduce audible content, but it exercises the exact sample-rate/buffer geometry observed in REAPER for longer than the previous short lifecycle cases.

## Workflow protection

Main workflow now requires both packaged B5 DLLs to contain:

- `dual-event-mux-v3`
- `runtime-failsafe-v1`

The validated B4D ancestry/frozen-core check remains unchanged.

## Next action

1. run manual `Build ASIO B5 Productization`;
2. require ARM64EC + Classic ARM64 build PASS and both runtime markers PASS;
3. run `install_and_validate_b5.cmd` once;
4. confirm the new `reaper-48-480-output` case passes;
5. then do one normal REAPER 48k/480 audible playback test;
6. do not intentionally provoke or repeatedly reproduce the loud-buzz failure;
7. if playback fails again, stop testing and return `%TEMP%\B5_RUNTIME_FAILURE.txt` immediately.

ASIO control-panel work remains planned, but this concrete real-playback safety regression takes priority until the new fail-safe build is validated and an actual failure reason is captured if the symptom recurs.

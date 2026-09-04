# Sound Blaster X4 ARM64 ASIO B5 productization

B5 is derived from the hardware/user-proven B4D baseline but is registered side-by-side so the proven B4D driver remains available during validation.

## Measured reference contract

The 2026-09-04 B5 capability capture established the following Creative `SB USB RT ASIO` behavior and X4 KS ranges:

- Creative public ASIO channels: 2 inputs / 10 outputs
- Creative ASIO sample type: `Int24LSB` (type 17) on every exposed channel
- Creative buffer contract: min 96, max 4800, preferred 240, granularity 48 frames
- Creative supported sample rates: 48000, 96000, 192000 Hz
- Creative clock: one `Internal Clock`
- Creative latency at preferred buffer: 240 input / 240 output frames
- Creative repeated create/start/stop/dispose lifecycle: PASS x3
- X4 Render Pin 1: stereo/6ch/8ch, 16/24-bit, 48/96/192 kHz
- X4 Capture Pin 4: stereo, 16/24-bit, 48/96 kHz

A direct WaveRT geometry probe measured an additional allocation constraint on the Windows `msft_wave` path at 192 kHz / stereo / 24-bit / `NotificationCount=2`:

- 48..336 frames per notification: rejected with `Win32=87`
- 384 frames per notification (2.0 ms): first accepted geometry
- every tested 432..960 frame candidate: accepted
- accepted candidates returned `ActualBufferSize == RequestedBufferSize`

B5 intentionally remains narrow:

- 2 output channels only
- 2 input channels only
- `Int24LSB` host format
- output: 48/96/192 kHz
- input: 48/96 kHz; at 192 kHz `getChannels()` reports zero inputs
- 48/96 kHz ASIO buffer contract: min 96, max 4800, preferred 240, granularity 48 frames
- 192 kHz ASIO buffer contract: min 384, max 4800, preferred 384, granularity 48 frames
- 512 frames is also accepted as an undocumented compatibility exception for existing B4D-era host settings

Broad multichannel output is still deferred.

## Safety

The historical active-render collision must never be recreated.

B5 keeps the Render Pin 1 local/global instance gate at `init()` and the WaveRT engine re-checks the relevant pin immediately before every `KsCreatePin`. Capture Pin 4 has its own local/global gate. BUSY or indeterminate state blocks the open; there is no override.

The B5 validation driver uses a different CLSID and ASIO registry entry:

`Sound Blaster X4 ARM64 ASIO B5`

The existing proven B4D entry remains untouched.

Render/capture copy failure, callback-index repetition, render-position regression, unrecoverable packet discontinuity and joined-worker shutdown checks remain strict.

## Runtime worker failure fail-safe

A real REAPER playback test exposed a failure mode in which audio could turn into a sustained repeating `drone/buzz` while the host itself remained alive. A worker fatal exit could leave the WaveRT render pin in RUN with the last cyclic contents still audible.

B5 carries runtime marker:

`runtime-failsafe-v1`

On any fatal mux/worker path:

1. the pre-failure render/capture stats and error strings are snapshotted;
2. both WaveRT render notification slots are overwritten with silence through the existing render copy API;
3. no KSSTATE transition, pin close, dispose, or hardware teardown occurs inside the failing worker;
4. the worker returns and the existing joined-worker-before-teardown rule remains authoritative;
5. a one-shot failure record is emitted through `OutputDebugString` and written to `%TEMP%\B5_RUNTIME_FAILURE.txt`.

File/debug logging happens only after the emergency silence attempt. Captured fatal runs have directly shown `emergencySilence=OK`.

## Render notification coalescing — mux v4

Runtime evidence first showed a render PACKETCOUNT transition `332 -> 334` at 192 kHz / 384 frames. A later run then showed the same exact forward `+2` pattern at 48 kHz / 240 frames:

`previous=74 expected=75 current=76 delta=2`

The 48 kHz result rules out a 192 kHz-only minimum-period explanation. The shared mechanism is the WaveRT auto-reset notification event: it is not a counting semaphore, so one event state can represent more than one elapsed hardware packet if user mode services it late.

Current runtime marker:

`dual-event-mux-v4-coalesce-recovery`

Mux v4 treats exactly one measured forward `delta=2` render transition as one explicit xrun/coalesced notification, not as packet corruption:

1. the WaveRT engine increments `notification_coalesces` rather than `packet_discontinuities`;
2. the mux invokes the missing ASIO callback for `currentPacket - 1` to preserve host double-buffer alternation and sample timeline;
3. output produced by that synthetic catch-up callback is deliberately discarded because its target hardware packet has already completed;
4. in duplex mode the synthetic input block is zero-filled and does not consume capture staging;
5. the normal current-packet callback then runs and writes `currentPacket + 1`, which is still a future WaveRT packet;
6. the recovery is counted as `renderCoalesces` / `renderDroppedBlocks` and surfaced in worker diagnostics.

This is deliberately narrow. The following remain fatal:

- render duplicate/backward packet transitions
- render forward jumps larger than `delta=2`
- capture packet discontinuity
- render position regression
- callback-index repetition outside the explicit catch-up sequence
- render/capture copy failure
- staging failure or sustained capture starvation
- worker failure

No ASIO host reset request is issued by this first recovery implementation. The catch-up callback preserves the existing ASIO timeline without introducing a host restart side effect.

## Full-duplex timing

Render remains the ASIO callback/master clock while Capture runs as an independent producer. Capture packets are staged and the oldest available staged packet is supplied to the current ASIO input buffer. `ERROR_NOT_READY` remains transient, `MoreData=TRUE` is drained immediately, and real capture packet discontinuity remains fatal.

For input-only operation, capture notifications drive the ASIO callback directly.

## One-shot product validation

Run `install_and_validate_b5.cmd` with REAPER/other X4 playback closed and the Windows default output moved away from X4 if necessary.

The script:

1. registers B5 side-by-side;
2. verifies registration;
3. checks the immutable Render Pin 1 idle gate;
4. runs the bundled silent lifecycle matrix covering:
   - 48 kHz / 240 frames / output, 3 reopen cycles
   - 48 kHz / 240 frames / full duplex, 2 cycles
   - 96 kHz / 240 frames / full duplex, 2 cycles
   - 48 kHz / 480 frames / output for 5 seconds (matches the observed REAPER host buffer)
   - 192 kHz / 384 frames / output, 2 cycles
   - 48 kHz / 96 frames / output
   - 48 kHz / 4800 frames / output
   - 48 kHz / 512 frames / compatibility output
5. captures the B5 public ASIO contract after lifecycle PASS.

Output:

`B5_PRODUCT_VALIDATION_REPORT.txt`

A successful v4 run may legitimately show non-zero `renderCoalesces` / `renderDroppedBlocks` while still returning `stop=0`, provided strict packet discontinuities, position/index/copy failures and worker failure remain zero. That represents an observed one-block xrun that the driver recovered from rather than a silent integrity pass.

If the first idle gate is BUSY or indeterminate, the script stops before lifecycle work. Do not bypass it.

## 192 kHz cadence probe status

`probe_b5_192k_cadence.cmd` remains packaged as a diagnostic tool, but the later 48 kHz `74 -> 76` evidence supersedes the earlier idea that this was primarily a 192 kHz buffer-period problem. Do not raise the 192 kHz public minimum merely to hide a notification coalescing event.

The immediate validation target is mux-v4 recovery across the normal full product matrix. Use the dedicated cadence probe only if later evidence again isolates a sample-rate-specific problem.

After the bundled validation passes, perform one normal REAPER 48 kHz / 480 audible playback test. If a fatal failure occurs, do not repeatedly retry; immediately collect `%TEMP%\B5_RUNTIME_FAILURE.txt`.

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

A later strict RUN validation observed a render PACKETCOUNT skip at 192 kHz / 384 frames. Therefore 384 is proven allocation-valid but is not yet proven sustained-cadence stable. Do not treat the current 384-frame public contract as final until the RUN cadence probe below returns.

B5 intentionally remains narrow:

- 2 output channels only
- 2 input channels only
- `Int24LSB` host format
- output: 48/96/192 kHz
- input: 48/96 kHz; at 192 kHz `getChannels()` reports zero inputs
- 48/96 kHz ASIO buffer contract: min 96, max 4800, preferred 240, granularity 48 frames
- current 192 kHz ASIO buffer contract: min 384, max 4800, preferred 384, granularity 48 frames
- 512 frames is also accepted as an undocumented compatibility exception for existing B4D-era host settings

Broad multichannel output is still deferred.

## Safety

The historical active-render collision must never be recreated.

B5 keeps the Render Pin 1 local/global instance gate at `init()` and the WaveRT engine re-checks the relevant pin immediately before every `KsCreatePin`. Capture Pin 4 has its own local/global gate. BUSY or indeterminate state blocks the open; there is no override.

The B5 validation driver uses a different CLSID and ASIO registry entry:

`Sound Blaster X4 ARM64 ASIO B5`

The existing proven B4D entry remains untouched.

Packet discontinuity, render position regression, callback-index repetition, render/capture copy failure and joined-worker shutdown checks remain strict. Do not weaken them merely to make validation pass.

## Runtime worker failure fail-safe

A real REAPER playback test exposed a failure mode in which audio could turn into a sustained repeating `drone/buzz` while the host itself remained alive. The leading mechanism is a worker failure leaving the WaveRT render pin in RUN with the last cyclic contents still audible.

B5 now carries runtime marker:

`runtime-failsafe-v1`

On any fatal mux/worker path:

1. the pre-failure render/capture stats and error strings are snapshotted;
2. both WaveRT render notification slots are overwritten with silence through the existing render copy API;
3. no KSSTATE transition, pin close, dispose, or hardware teardown occurs inside the failing worker;
4. the worker returns and the existing joined-worker-before-teardown rule remains authoritative;
5. a one-shot failure record is emitted through `OutputDebugString` and written to:

`%TEMP%\B5_RUNTIME_FAILURE.txt`

The record contains rate, frames, callback/index/copy counters, render/capture packet and position counters, capture phase counters, engine messages, Win32 error value, and whether emergency silence succeeded.

File/debug logging happens only after the emergency silence attempt so filesystem latency cannot prolong a repeating last-buffer tone.

The first captured real fatal record proved `emergencySilence=OK` on a 192 kHz render PACKETCOUNT discontinuity.

## Exact packet discontinuity diagnostics

The signaled WaveRT path records a non-sequential packet as:

`previous=<n> expected=<n+1> current=<m> delta=<m-n>`

Render position regression diagnostics similarly record previous/current positions.

The strict failure behavior is unchanged; this only improves diagnosis.

## Full-duplex timing

Mux v3 keeps Render as the ASIO callback/master clock while Capture runs as an independent producer. Capture packets are staged and the oldest available staged packet is supplied to the current ASIO input buffer. A short render/capture phase difference no longer causes an immediate false synchronization failure, while real packet discontinuity, copy failure, staging failure, sustained capture starvation, callback-index repetition, render-position regression, and worker failure remain fatal.

For input-only operation, capture notifications drive the ASIO callback directly.

## One-shot product validation

Run `install_and_validate_b5.cmd` with REAPER/other X4 playback closed and the Windows default output moved away from X4 if necessary.

The script:

1. registers B5 side-by-side;
2. verifies registration;
3. checks the immutable Render Pin 1 idle gate;
4. runs one bundled silent lifecycle matrix with sample-rate-specific buffer-contract checks covering:
   - 48 kHz / 240 frames / output, 3 reopen cycles
   - 48 kHz / 240 frames / full duplex, 2 cycles
   - 96 kHz / 240 frames / full duplex, 2 cycles
   - 48 kHz / 480 frames / output for 5 seconds (matches the observed REAPER host buffer)
   - 192 kHz / 384 frames / output, 2 cycles
   - 48 kHz / 96 frames / output
   - 48 kHz / 4800 frames / output
   - 48 kHz / 512 frames / compatibility output
5. captures the B5 public ASIO contract after lifecycle PASS.

The REAPER-matched 48k/480 case is intentionally before the current 192 kHz case so a later 192 kHz failure cannot hide that host geometry result.

Output:

`B5_PRODUCT_VALIDATION_REPORT.txt`

If the first idle gate is BUSY or indeterminate, the script stops before lifecycle work. Do not bypass it.

## Dedicated 192 kHz RUN cadence probe

The current 192 kHz allocation minimum is 384 frames / 2.0 ms, but one strict runtime cycle observed a packet transition consistent with `332 -> 334`. Use the cadence probe before changing the public contract.

Run:

`probe_b5_192k_cadence.cmd`

This invokes:

`x4-asio-stage-b5-product-validation.exe --cadence-192`

Candidates:

- 384 frames = 2.000 ms, 1 x 5 seconds
- 432 frames = 2.250 ms, 2 x 10 seconds
- 480 frames = 2.500 ms, 2 x 10 seconds
- 576 frames = 3.000 ms, 2 x 10 seconds

The normal fatal packet-discontinuity checks remain enabled for every candidate. A safely joined candidate failure does not prevent later candidates from being measured; BUSY still aborts the probe.

Output:

`B5_192K_CADENCE_REPORT.txt`

Choose any future 192 kHz minimum/preferred change only from the first sustained stable candidate measured by this probe. If larger candidates also fail, diagnose notification scheduling rather than blindly raising the buffer.

After the bundled validation and cadence issue are resolved, REAPER should show both the existing B4D entry and `Sound Blaster X4 ARM64 ASIO B5` for real playback/input validation.

If real playback fails again, do not repeatedly retry. Immediately collect `%TEMP%\B5_RUNTIME_FAILURE.txt` and use that record as the next source of truth.

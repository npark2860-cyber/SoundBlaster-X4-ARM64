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

A later direct WaveRT geometry probe measured an additional constraint on the Windows `msft_wave` path at 192 kHz / stereo / 24-bit / `NotificationCount=2`:

- 48..336 frames per notification: rejected with `Win32=87`
- 384 frames per notification (2.0 ms): first accepted geometry
- every tested 432..960 frame candidate: accepted
- accepted candidates returned `ActualBufferSize == RequestedBufferSize`

Therefore the first-release B5 ASIO contract intentionally differs from Creative's public 240-frame preferred value at 192 kHz in order to preserve the simple and already-validated 1:1 ASIO-buffer-to-WaveRT-packet model.

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

## Full-duplex timing

Mux v3 keeps Render as the ASIO callback/master clock while Capture runs as an independent producer. Capture packets are staged and the oldest available staged packet is supplied to the current ASIO input buffer. A short render/capture phase difference no longer causes an immediate false synchronization failure, while real packet discontinuity, copy failure, staging failure, sustained capture starvation, callback-index repetition, render-position regression, and worker failure remain fatal.

For input-only operation, capture notifications drive the ASIO callback directly.

## One-shot validation

Run `install_and_validate_b5.cmd` with REAPER/other X4 playback closed and the Windows default output moved away from X4 if necessary.

The script:

1. registers B5 side-by-side;
2. verifies registration;
3. checks the immutable Render Pin 1 idle gate;
4. runs one bundled silent lifecycle matrix with sample-rate-specific buffer-contract checks covering:
   - 48 kHz / 240 frames / output, 3 reopen cycles
   - 48 kHz / 240 frames / full duplex, 2 cycles
   - 96 kHz / 240 frames / full duplex, 2 cycles
   - 192 kHz / 384 frames / output, 2 cycles
   - 48 kHz / 96 frames / output
   - 48 kHz / 4800 frames / output
   - 48 kHz / 512 frames / compatibility output
5. captures the B5 public ASIO contract after lifecycle PASS.

Output:

`B5_PRODUCT_VALIDATION_REPORT.txt`

If the first idle gate is BUSY or indeterminate, the script stops before lifecycle work. Do not bypass it.

After the bundled validation passes, REAPER should show both the existing B4D entry and `Sound Blaster X4 ARM64 ASIO B5` for real playback/input validation.

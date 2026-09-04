# DEBUG HISTORY — ASIO B5 capability runtime BUSY-blocked capture

Date: 2026-09-04 KST

## Context

B5 capability tooling was built and executed on the X4 test system. The generated `B5_CAPABILITY_REPORT.txt` reached the property-only KS probe, enumerated the X4 pin topology and audio data ranges, then stopped at the immutable Render Pin 1 ownership gate before any Creative/independent ASIO lifecycle probe was allowed to run.

## Safety result

The gate observed:

- Render Pin 1 local instances: `C 0/1`
- Render Pin 1 global instances: `G 1/1`
- result: `BUSY=YES`
- `KsCreatePin` was never called by the B5 KS probe
- combined probe result: `BLOCKED BY IDLE/BUSY GATE`

This is the intended behavior. Do not weaken or bypass this gate.

The `C 0/1, G 1/1` pattern means the current filter handle owns no Render Pin 1 instance, while another filter/process context already consumes the only globally available instance. The probe therefore cannot safely continue into any ASIO lifecycle work.

## Static KS capability evidence captured before the stop

Device path:

`vid_041e&pid_3278&mi_03 ... \msft_wave`

Pin count: 6.

### Render Pin 1

Dataflow: render-to-device.

Advertised PCM ranges include:

- channels: 2, 6, 8
- bit depths: 16-bit, 24-bit
- sample rates: 48 kHz, 96 kHz, 192 kHz

Thus the X4 KS layer statically advertises the full cross-product represented by its ranges for 2/6/8-channel 16/24-bit PCM at 48/96/192 kHz.

### Render Pin 3

Dataflow: render-to-device.

Advertised PCM ranges include stereo:

- 16-bit at 48/96/192 kHz
- 24-bit at 48/96/192 kHz

There is also one additional 16-bit 48 kHz range with subtype `{00000092-0000-0010-8000-00AA00389B71}`. Do not reinterpret this range without a specific need.

### Capture Pin 4

Dataflow: capture-from-device.

Advertised PCM ranges:

- stereo 16-bit at 48 kHz and 96 kHz
- stereo 24-bit at 48 kHz and 96 kHz

This is the strongest current static candidate for the first narrow stereo input implementation, but runtime ownership and ASIO format behavior must still be measured before implementation.

### Pins 0, 2, 5

These expose non-PCM bridge/topology-style ranges rather than ordinary PCM audio ranges in this capture. They are not selected for first B5 PCM transport work.

## What remains unknown

Because the BUSY gate stopped the combined script before the Creative COM probe, the following B5 specification inputs are still unmeasured:

- Creative input/output channel counts and names
- Creative raw ASIO sample types, including exact 24-bit packing contract
- Creative buffer min/max/preferred/granularity
- Creative `canSampleRate()` matrix
- Creative latency reporting
- Creative clock sources
- Creative repeated create/start/stop/dispose/reopen behavior
- matching independent-driver capability/lifecycle report

Do not implement 24-bit packing, selectable buffers, or the final rate/input contract by assumption from KS ranges alone.

## Next action

Make the X4 Render Pin 1 globally idle, then run the existing packaged `probe_b5.cmd` one more time as a single combined capture.

Preferred preparation:

1. switch Windows default output away from the X4 to another playback device;
2. close REAPER, foobar/other media players, Creative App, and any browser/media process actively using the X4;
3. do not disable or alter the BUSY gate;
4. run `probe_b5.cmd` once;
5. return the new `B5_CAPABILITY_REPORT.txt`.

If the report still shows `G 1/1`, do not retry blindly. Add ownership diagnostics instead of weakening the gate.

## Branch/source state

Validated B4D remains:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

B5 measurement branch remains:

`exp/windows-arm64-asio-b5-capability-productization@bf5039e57ad0617db2e14269389f62c7e046bcb7`

No B5 transport/productization changes were made from this BUSY-blocked report.

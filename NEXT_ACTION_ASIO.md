# NEXT ACTION — Native ARM64 ASIO engine

Updated: 2026-09-03 KST

## Current status

ASIO feasibility is no longer the primary uncertainty.

Hardware-confirmed on the user's Windows ARM64 Sound Blaster X4:

- Microsoft USB Audio 2.0 `msft_wave` KS filter is accessible from user mode
- Render Pin 1 can be created at 48 kHz / stereo / 16-bit PCM
- `KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION` allocates a real cyclic buffer
- requested 4096 bytes -> actual 4096 bytes
- notification event registration succeeds
- `KSSTATE_ACQUIRE`, `PAUSE`, `RUN`, and `STOP` all succeed
- 20/20 DMA notifications were received
- `KSPROPERTY_RTAUDIO_PACKETCOUNT` advanced 1..20
- `KSPROPERTY_RTAUDIO_PRESENTATION_POSITION` advanced during RUN
- presentation position advanced by 512 frames per notification after startup
- 512 frames at 48 kHz = 10.666666... ms
- 4096-byte stereo 16-bit buffer = 1024 frames, so notification count 2 correctly produces 512-frame half-buffer cadence

Hardware-mapped `POSITIONREGISTER` and `CLOCKREGISTER` returned `ERROR_NOT_SUPPORTED`; do not require them. The working packet/presentation-position fallback is sufficient for the next prototype.

See:

`DEBUG_HISTORY_20260903_ASIO_WAVERT_ACTIVE_RUNTIME_SUCCESS.md`

## Immediate implementation target

Build the first independent ASIO engine prototype around the proven WaveRT render path.

Deliberately narrow Stage A scope:

1. Sound Blaster X4 `msft_wave` filter discovery
2. Render Pin 1 only
3. 48 kHz only
4. stereo only
5. 16-bit PCM only
6. 4096-byte WaveRT cyclic buffer
7. notification count = 2
8. expose two logical host buffers of 512 frames each
9. callback/buffer index flips once per DMA notification
10. sample position derived from presentation position, with packet count as continuity evidence
11. clean STOP/unregister/close

Do not add capture, multichannel, 24-bit, 96/192 kHz, dynamic buffer sizes, or sample-rate switching in Stage A.

## Stage A success criteria

The prototype must prove:

- stable callback alternation 0/1/0/1...
- no missed packet-count increments during a sustained run
- monotonically advancing sample position
- clean start/stop across repeated runs
- no dependency on Creative user-mode DLLs
- no dependency on Creative x64 ASIO DLLs

## After Stage A

Stage B:

- implement the ASIO COM interface/registration around the proven render engine
- expose it to a real ASIO host
- verify host buffer callbacks and sample position

Stage C:

- add Capture Pin 4
- verify capture notification cadence independently
- then full-duplex synchronization

Stage D:

- add 24-bit format handling
- 96/192 kHz where exposed
- multichannel render where exposed
- configurable ASIO buffer sizes
- sample-rate switching and reset notifications

## Architectural rule

Final product code must be independent native ARM64 code.

Creative binaries are reference material only. Do not load or redistribute `CtU2As64.dll`, `CTCDC.dll`, `CTIntrfu.dll`, or Creative application assemblies as runtime dependencies.

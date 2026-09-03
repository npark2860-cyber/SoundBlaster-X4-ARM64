# DEBUG HISTORY — X4 ASIO KS/WaveRT Runtime Capability

Date: 2026-09-03 KST

## Scope

Hardware runtime validation of the Windows ARM64 X4 audio stack for a future native ARM64 ASIO implementation.

This test did not allocate an audio buffer and did not send KSSTATE_ACQUIRE / PAUSE / RUN. It enumerated the X4 KS audio filters, opened compatible streaming pin instances, and queried KSPROPSETID_RtAudio BASICSUPPORT.

## Device/filter

Sound Blaster X4

Hardware IDs:

- `USB\VID_041E&PID_3278&REV_1070&MI_03`
- `USB\VID_041E&PID_3278&MI_03`

Wave filter path suffix:

`{6994ad04-93ef-11d0-a3cc-00a0c9223196}\msft_wave`

A second topology filter (`msft_topo`) was also enumerated but exposed no streamable pins in this probe.

## Streaming pins successfully opened

Three streaming pin instances were created successfully with `KsCreatePin` using a safe 48 kHz stereo 16-bit PCM test format:

- Pin 1 — dataflow IN, communication SINK (render path)
- Pin 3 — dataflow IN, communication SINK (render path)
- Pin 4 — dataflow OUT, communication SINK (capture path)

All three used standard looped-streaming interface ID 1 and standard medium ID 0.

## Hardware-confirmed WaveRT support

On all three successfully opened pin instances, `KSPROPSETID_RtAudio` BASICSUPPORT reported support for:

- `KSPROPERTY_RTAUDIO_BUFFER`
- `KSPROPERTY_RTAUDIO_HWLATENCY`
- `KSPROPERTY_RTAUDIO_POSITIONREGISTER`
- `KSPROPERTY_RTAUDIO_CLOCKREGISTER`
- `KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION`
- `KSPROPERTY_RTAUDIO_REGISTER_NOTIFICATION_EVENT`
- `KSPROPERTY_RTAUDIO_UNREGISTER_NOTIFICATION_EVENT`
- `KSPROPERTY_RTAUDIO_QUERY_NOTIFICATION_SUPPORT`

Observed access flags:

- read-only properties above: `0x00000001`
- register/unregister notification event: `0x00000003`

Summary from runtime:

- successfully opened streaming pin instances: 3
- pin instances with `BUFFER_WITH_NOTIFICATION` BASICSUPPORT: 3

## Format evidence

The render path exposed PCM ranges including:

- 48 / 96 / 192 kHz
- 16-bit and 24-bit
- 2 / 6 / 8 channels on Pin 1
- 2 channels on Pin 3

The capture path Pin 4 exposed PCM ranges including:

- 48 / 96 kHz
- 16-bit and 24-bit
- 2 channels

Do not over-generalize the first probe into the final ASIO format matrix; these are only the ranges observed in this runtime enumeration.

## Conclusion

This is strong hardware evidence that a native ARM64 ASIO implementation can be built as a user-mode ASIO COM DLL over the existing Windows KS/WaveRT path:

`ARM64 ASIO host`
→ `new ARM64 ASIO COM DLL`
→ `KsCreatePin / KSPROPSETID_RtAudio`
→ `Microsoft USB Audio 2.0 / WaveRT path`
→ `Sound Blaster X4`

The key obstacle is no longer whether the current ARM64 X4 audio stack exposes the required low-latency primitives. It does.

The remaining work is implementation and timing validation:

1. request a real `KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION` buffer
2. register notification events
3. read clock / position / hardware latency values
4. transition a test render pin through ACQUIRE / PAUSE / RUN / STOP
5. validate notification cadence and DMA position movement
6. only then map that machinery onto the ASIO callback model

## Important restraint

This result does not yet prove glitch-free low-latency ASIO performance, DAW compatibility, sample-rate switching behavior, or final latency values. Those require the next runtime stages.

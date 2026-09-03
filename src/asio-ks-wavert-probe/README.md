# Sound Blaster X4 ASIO KS/WaveRT capability probe

Experimental branch for determining whether the Windows ARM64 X4 audio stack exposes the standard KS/WaveRT capabilities needed by a new native ARM64 ASIO implementation.

## Scope

This stage is capability discovery only.

The temporary runtime probe:

1. enumerates `KSCATEGORY_AUDIO` interfaces
2. identifies Sound Blaster X4 / `VID_041E&PID_3278` candidates
3. queries `KSPROPSETID_Pin` metadata
4. briefly creates compatible render/capture pin instances with `KsCreatePin`
5. queries `KSPROPSETID_RtAudio` using `KSPROPERTY_TYPE_BASICSUPPORT`
6. closes the pin

It does **not** allocate a WaveRT audio buffer, register notification events, or transition a pin to `KSSTATE_ACQUIRE`, `PAUSE`, or `RUN`.

## Primary properties under test

- `KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION`
- `KSPROPERTY_RTAUDIO_CLOCKREGISTER`
- `KSPROPERTY_RTAUDIO_POSITIONREGISTER`
- `KSPROPERTY_RTAUDIO_HWLATENCY`

Also logged:

- `KSPROPERTY_RTAUDIO_BUFFER`
- `KSPROPERTY_RTAUDIO_REGISTER_NOTIFICATION_EVENT`
- `KSPROPERTY_RTAUDIO_UNREGISTER_NOTIFICATION_EVENT`
- `KSPROPERTY_RTAUDIO_QUERY_NOTIFICATION_SUPPORT`

## Interpretation discipline

A positive result is strong evidence that an independent ARM64 ASIO COM DLL can use the standard Windows KS/WaveRT path to the X4.

A negative result at this stage is **not** proof of impossibility. It may mean the probe selected a different pin/interface/format than Creative's existing ASIO implementation.

Creative binaries are reference material only and are not runtime dependencies of the intended ARM64 implementation.

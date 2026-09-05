# DEBUG HISTORY — 2026-09-05 MalLgcy Native Forward Trace

Branch:

`exp/windows-arm64-x4-native-controller`

## Scope

This trace analyzes the supplied `MalLgcy.dll` to close the native side of the ordinary Windows Mixer path identified from `Creative.Platform.Mixer.dll`.

No hardware runtime probe was added, no mixer SET was issued, and no B5 ASIO code was modified.

## Binary identity

Supplied binary:

- `MalLgcy.dll` SHA-256 `bf2ba6d85fa1cdf20a2fa866d153cefa1e5e7f9af87107d83963ed393e4591aa`
- PE format: `coff-i386` / PE32
- Machine: x86 / 32-bit
- PE timestamp: 2014-10-30 09:17:54 UTC as stored in the header

This exact binary therefore cannot be loaded directly into an ARM64 native process.

## Result summary

`MalLgcy.dll` is not the implementation layer that performs the actual endpoint/topology volume math.

For the Mixer functions relevant to the X4 controller, `MalLgcy` exports `CSCT*` entry points that jump to thin wrapper bodies. Those bodies push the original arguments unchanged and call matching imports from `CTAudEp.dll`.

No scalar conversion, dB conversion, fixed-point conversion, channel remapping, CDC framing, or X4-specific AudioLevel translation occurs in these wrappers.

The native implementation boundary is therefore:

`Creative.Platform.Mixer.dll`
-> `MalLgcy.dll!CSCT*`
-> `CTAudEp.dll!CT*`

For an ARM64 native controller, the important contract is the endpoint/topology behavior behind the `CTAudEp` calls, not reuse of this x86 `MalLgcy.dll` binary.

## Exact forwarding recovered

### Endpoint master/channel volume

`CSCTGetMasterVolume`
-> `CTAudEp!CTGetMasterVolume(endpointId, fScalar, float*)`

`CSCTSetMasterVolume`
-> `CTAudEp!CTSetMasterVolume(endpointId, fScalar, float, eventContext)`

`CSCTGetChannelVolume`
-> `CTAudEp!CTGetChannelVolume(endpointId, fScalar, channel, float*)`

`CSCTSetChannelVolume`
-> `CTAudEp!CTSetChannelVolume(endpointId, fScalar, channel, float, eventContext)`

The x86 wrapper code performs no arithmetic before the imported call.

### Monitoring controls

`CSCTOpenMonitoringControl`
-> `CTAudEp!CTOpenMonitoringControl`

`CSCTGetVolumeChannelCountOfMonitoringControl`
-> `CTAudEp!CTGetVolumeChannelCountOfMonitoringControl`

`CSCTGetVolumeLevelRangeOfMonitoringControl`
-> `CTAudEp!CTGetVolumeLevelRangeOfMonitoringControl(handle, channel, min*, max*, stepping*)`

`CSCTGetHighestVolumeLevelOfMonitoringControl`
-> `CTAudEp!CTGetHighestVolumeLevelOfMonitoringControl(handle, fScalar, float*)`

`CSCTGetVolumeLevelOfMonitoringControl`
-> `CTAudEp!CTGetVolumeLevelOfMonitoringControl(handle, channel, fScalar, float*)`

`CSCTSetVolumeLevelUniformOfMonitoringControl`
-> `CTAudEp!CTSetVolumeLevelUniformOfMonitoringControl`

`CSCTSetVolumeLevelOfMonitoringControl`
-> `CTAudEp!CTSetVolumeLevelOfMonitoringControl`

`CSCTGetMuteOfMonitoringControl`
-> `CTAudEp!CTGetMuteOfMonitoringControl`

`CSCTSetMuteOfMonitoringControl`
-> `CTAudEp!CTSetMuteOfMonitoringControl`

`CSCTCloseMonitoringControl`
-> `CTAudEp!CTCloseMonitoringControl`

Again, the MalLgcy wrapper passes the original arguments directly. In particular, the range wrapper passes `handle`, `channel`, `min*`, `max*`, and `stepping*` unchanged.

### KS node type volume / Mic Boost path

`CSCTOpenKsNodeTypeVolumeOfAudioEndpoint`
-> `CTAudEp!CTOpenKsNodeTypeVolumeOfAudioEndpoint`

`CSCTGetLevelRangeOfKsNodeTypeVolumeOfAudioEndpoint`
-> `CTAudEp!CTGetLevelRangeOfKsNodeTypeVolumeOfAudioEndpoint(handle, min*, max*, stepping*)`

`CSCTGetLevelOfKsNodeTypeVolumeOfAudioEndpoint`
-> `CTAudEp!CTGetLevelOfKsNodeTypeVolumeOfAudioEndpoint(handle, fScalar, float*)`

`CSCTSetLevelOfKsNodeTypeVolumeOfAudioEndpoint`
-> `CTAudEp!CTSetLevelOfKsNodeTypeVolumeOfAudioEndpoint`

`CSCTCloseKsNodeTypeVolumeOfAudioEndpoint`
-> `CTAudEp!CTCloseKsNodeTypeVolumeOfAudioEndpoint`

No conversion is performed inside MalLgcy before forwarding.

### KS node Auto Gain Control / Mic AGC path

`CSCTOpenKsNodeTypeAutoGainControlOfAudioEndpoint`
-> `CTAudEp!CTOpenKsNodeTypeAutoGainControlOfAudioEndpoint`

`CSCTGetStateOfKsNodeTypeAutoGainControlOfAudioEndpoint`
-> `CTAudEp!CTGetStateOfKsNodeTypeAutoGainControlOfAudioEndpoint`

`CSCTSetStateOfKsNodeTypeAutoGainControlOfAudioEndpoint`
-> `CTAudEp!CTSetStateOfKsNodeTypeAutoGainControlOfAudioEndpoint`

`CSCTCloseKsNodeTypeAutoGainControlOfAudioEndpoint`
-> `CTAudEp!CTCloseKsNodeTypeAutoGainControlOfAudioEndpoint`

## Relationship to the managed `fScalar` result

The previous managed static trace established that:

- `MixerLine` uses `fScalar=true` for endpoint master/channel levels;
- `MonitorLine` uses `fScalar=true` for monitoring levels;
- Mic Boost uses the KS volume path with `fScalar=false`;
- managed scalar values are converted between 0..1 and UI 0..100.

The MalLgcy native trace now confirms there is no hidden conversion layer between that managed call and `CTAudEp`.

The `fScalar` flag and float value reach `CTAudEp.dll` unchanged.

Therefore the next binary required to identify the actual Windows Core Audio / DeviceTopology / KS implementation is `CTAudEp.dll`.

## CDC raw UInt16 engineering-unit question

This DLL does not close the remaining CDC `0x22/0x23` raw `UInt16` conversion question.

Evidence:

- `MalLgcy.dll` has no dependency on `CTCDC.dll` for these Mixer functions;
- the relevant volume/monitoring/KS wrappers forward directly to `CTAudEp.dll`;
- no raw `UInt16` fixed-point conversion appears in the forwarding layer.

Therefore do not spend further time looking for `/256` or another CDC conversion inside MalLgcy.

The exact CDC Game/Voice raw-unit conversion must be sought in a layer that actually consumes the `CDCGameVoice` `UInt16` values, most likely a higher Creative Platform/App consumer or another dedicated helper.

## ARM64 consequence

This supplied `MalLgcy.dll` is x86 PE32.

An ARM64-native controller cannot load this exact DLL in-process.

That does not block implementation because the required ordinary mixer operations are standard endpoint/topology concepts exposed through the `CTAudEp` API contract:

- endpoint master/channel volume;
- endpoint mute;
- monitoring topology volume/mute;
- KS-node volume for Mic Boost;
- KS-node AGC.

The intended ARM64 implementation should recover/reproduce those operations with native Windows Core Audio / DeviceTopology / KS APIs rather than depend on this x86 legacy wrapper.

Do not create an x86 broker solely to preserve MalLgcy unless later evidence shows Creative-specific behavior that cannot be reproduced directly.

## Next static target

Primary next binary:

- `CTAudEp.dll`

Goals:

1. identify the exact COM/Core Audio interfaces used for master/channel volume;
2. identify how monitoring controls are discovered in DeviceTopology;
3. identify the KS node/property mechanism used for Mic Boost and AGC;
4. record GUIDs/property IDs/node-subtype matching needed for a native ARM64 implementation;
5. determine whether any Creative-specific endpoint filtering or property-store behavior must also be reproduced.

Separately, continue searching higher Creative Platform/App code for the CDC Game/Voice raw `UInt16` engineering-unit conversion.

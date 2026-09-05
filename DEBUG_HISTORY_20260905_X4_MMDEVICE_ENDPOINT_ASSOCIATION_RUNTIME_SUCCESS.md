# DEBUG HISTORY — 2026-09-05 X4 MMDevice Endpoint Association Runtime Success

Branch:

`exp/windows-arm64-x4-native-controller`

## Scope

Read-only MMDevice endpoint inspection on the ARM64 X4 test machine. No property write, registry write, CTCDC command, driver install, APO registration or AudioDG change was performed.

## Runtime result

Six active Sound Blaster X4 MMDevice endpoints were found.

| Flow | Form factor | PKEY_AudioEndpoint_Association |
|---|---|---|
| Render | Speakers | GUID_NULL |
| Render | SPDIF | GUID_NULL |
| Capture | Microphone | GUID_NULL |
| Capture | UnknownDigitalPassthrough | GUID_NULL |
| Capture | SPDIF | GUID_NULL |
| Capture | LineLevel | GUID_NULL |

All endpoints reported state `DEVICE_STATE_ACTIVE` (`0x00000001`).

No distinct `Headphones` MMDevice endpoint exists in the current bare Microsoft `usbaudio2` state.

## Correlation with previous attachment probe

The preceding read-only attachment probe proved:

- X4 audio devnode `USB\VID_041E&PID_3278&MI_03`;
- class `MEDIA`;
- function service `usbaudio2`;
- `KSCATEGORY_AUDIO` interfaces `msft_wave` and `msft_topo`;
- runtime `msft_topo` pin categories including Speaker, SPDIF, Microphone, Line and Digital Audio Interface;
- no existing `FX` or `EP` subtree in the inspected bare-usbaudio2 interface/device registry locations.

The MMDevice result now adds that endpoint association properties are currently GUID_NULL and endpoint differentiation is provided by FormFactor rather than a non-null `PKEY_AudioEndpoint_Association` value.

## Consequence for the first live APO gate

Do not invent a Headphone endpoint and do not interpret Creative `FX\1` as KS pin 1.

For the first live pass-through-only gate, reduce scope to the one endpoint that is directly proven by both KS topology and MMDevice runtime:

- Render Speaker
- `KSNODETYPE_SPEAKER = {DFF21CE1-F70F-11D0-B917-00A0C9223196}`

The first candidate extension should therefore add only one effects association:

`PKEY_FX_Association = KSNODETYPE_SPEAKER`

and bind only the Stage A0 native ARM64 SFX/MFX/EFX identities.

Headphone, microphone, SPDIF, Line In, What-U-Hear and DDL remain excluded from the first live gate.

## Microsoft model used

Microsoft documents `PKEY_FX_Association` as the KS-node compatibility selector used by endpoint builder. The value is compared with the `KSPINDESCRIPTOR.Category` at the hardware end of the signal path. `KSNODETYPE_ANY` / GUID_NULL is a wildcard; a specific node type such as `KSNODETYPE_SPEAKER` restricts the APO association to that node type.

The componentized Windows 11 model remains:

1. device extension adds an APO software component;
2. `AudioProcessingObject` class INF installs/registers the native APO DLL;
3. interface FX metadata binds the APO to the selected endpoint graph.

## Safety

The runtime probe was read-only. This record does not authorize manual registry editing or direct installation of the earlier review INFs.

# DEBUG HISTORY — 2026-09-05 X4 SB1815 INF APO Binding / ARM64 Trace

Branch:

`exp/windows-arm64-x4-native-controller`

## Scope

This trace uses the supplied official Creative `ctusbaud.inf` to close the SB1815/X4 endpoint-to-APO binding and package-architecture questions that remained after the APO property-schema recovery.

No hardware write was performed. No runtime probe, controller source, or B5 ASIO code was modified.

## Source INF

- file: `ctusbaud.inf`
- SHA-256: `adc7b2128b9d90625efab36c6fc499d8d8f4328e368265f03222cf6720b98b0b`
- `DriverVer = 09/26/2024,3.06.03.00`
- class: `MEDIA`

SB1815 hardware match:

`USB\VID_041E&PID_3278&MI_03`

Product string:

`Sound Blaster X4`

The CTCDC control interface remains the separate `MI_01` path already documented elsewhere.

## 1. Architecture packaging result

The supplied Creative package has installation targets for:

- `ntx86`
- `ntamd64`

There are no `ntarm64`, ARM64 source-disk, catalog, install, service, copy-list, or AddReg sections.

The SB1815 amd64 Windows 11 install section is:

`USB2_Config_1815.ntamd64`

It includes the Microsoft USB Audio 2.0 class driver:

- `Include=usbaudio2.inf`
- `Needs=usbaudio2_Device.NT`

and additionally installs Creative components:

- `CTUSBfilt64.sys`
- `CTUSBAPO64.dll`
- `CTUSBAPO32.dll`
- `CTUSBWrap64.dll`
- `CTUSBDGFX64.dll`
- corresponding 32-bit compatibility binaries

The hardware section adds `CTUSBfilt64` as an `UpperFilters` entry.

Therefore the official package is not an ARM64 package with one missing binary; its install model is explicitly x86/amd64-only.

## 2. SB1815 endpoint model from INF

`USB_Config1815.SysEP.AddReg` defines the following physical/logical endpoint associations:

| EP slot | Association | Notes |
|---:|---|---|
| `EP\0` | `KSNODETYPE_SPEAKER` | render; Creative effect-node information present |
| `EP\1` | `KSNODETYPE_SPDIF_INTERFACE` | render; Creative effect-node information present |
| `EP\2` | `KSNODETYPE_MICROPHONE` | capture; Creative effect-node information present |
| `EP\3` | `KSNODETYPE_LINE_CONNECTOR` | capture; no Creative effect-node property in this section |
| `EP\4` | `KSNODETYPE_DIGITAL_AUDIO_INTERFACE` | What-U-Hear/custom endpoint; no Creative effect-node property in this section |

Creative effect-node definition properties:

- Render: `{F1056047-B091-4D85-A5C0-B13D4D8BAC57},0`
- Capture: `{F1056047-B091-4D85-A5C0-B13D4D8BAC57},1`

For SB1815 the value used is:

`{A14358D4-2952-4E26-8D27-8976993C4E61}`

which is the `CEffectNodeInfo` Creative COM class already identified in `CTUSBAPO64.dll`.

This ties the earlier `APODeviceFilter` product gate directly to the actual X4 INF.

## 3. Windows 11 FX binding — exact SB1815 map

The Windows 11 topology section uses:

`USB_Config1815.SysFx.AddReg`

### FX\0 — Speaker

Association/context: Speaker.

Creative APOs:

- SFX `{71DAB6A1-39F3-423E-90A8-032729851157}`
- MFX `{C624D7B2-8333-448E-85C8-51EEFC2025ED}`
- EFX `{EC2F4B76-6AE1-4DB9-8FF6-344B74CF9650}`

Legacy compatibility entries also point to:

- PreMix/LFX `{DA3AD2CF-79F9-41B7-BE44-753ADEEC2EDD}`
- PostMix/GFX `{CA854A19-6601-407B-8AFB-CB5C2801AFE6}`

All SFX/MFX/EFX streaming-mode lists contain only `AUDIO_SIGNALPROCESSINGMODE_DEFAULT` in this INF.

### FX\1 — Headphone

Association/context: Headphones.

Uses the same Creative SFX/MFX/EFX CLSIDs as Speaker:

- SFX `{71DAB6A1-39F3-423E-90A8-032729851157}`
- MFX `{C624D7B2-8333-448E-85C8-51EEFC2025ED}`
- EFX `{EC2F4B76-6AE1-4DB9-8FF6-344B74CF9650}`

### FX\2 — SPDIF Out

SPDIF uses a mixed Creative/chainer chain:

- SFX `{71DAB6A1-39F3-423E-90A8-032729851157}`
- MFX chainer `{6E623752-66A4-4281-BD29-D9DA22328623}`
- EFX chainer `{CC401F70-ACFB-4FBD-9F14-20E7CEF2E1A3}`

Additional chained-APO keys explicitly order:

1. Creative EFX `{EC2F4B76-6AE1-4DB9-8FF6-344B74CF9650}`
2. DGFX `{242249CC-E3C8-4571-9A0B-ED0906B7F994}`

The INF also selects digital encoder value `2`, commented as DDL.

This path depends on the additional `CTUSBWrap64.dll` / `CTUSBDGFX64.dll` stack and should be treated separately from the first ARM64 Speaker/Headphone/Mic APO target.

### FX\3 — Microphone

Association/context: Microphone.

Uses the same Creative SFX/MFX/EFX set:

- SFX `{71DAB6A1-39F3-423E-90A8-032729851157}`
- MFX `{C624D7B2-8333-448E-85C8-51EEFC2025ED}`
- EFX `{EC2F4B76-6AE1-4DB9-8FF6-344B74CF9650}`

The X4 microphone context also seeds Creative AEC-related configuration values in the user FX property subtree.

This statically closes the official CrystalVoice attachment point: X4 microphone processing is attached through the Windows APO FX graph, not solely through raw CTCDC `VoiceInputManager (0x95)`.

### FX\4 — Line In

Association: Line connector.

The INF uses Microsoft pre/post mix effects here rather than the Creative SFX/MFX/EFX set.

### FX\5 — What U Hear

Association: Digital audio interface.

Creative entries:

- SFX `{71DAB6A1-39F3-423E-90A8-032729851157}`
- MFX `{C624D7B2-8333-448E-85C8-51EEFC2025ED}`

No EFX CLSID is installed for FX\5 in the Windows 11 SB1815 section.

## 4. Windows 11 Audio System Effects contexts

The X4 INF contains explicit context property subtrees matching the recovered `IAudioSystemEffectsPropertyStore` model.

Creative general context:

`{852311BC-1AFB-454E-92CA-C35252CACAAF}`

Creative headphone context:

`{3F5F306B-A033-4F19-843D-1C44A736FF4D}`

Each context creates the standard sub-stores:

- `Default`
- `Volatile`
- `User`

This directly corroborates the recovered Platform call path:

`IAudioSystemEffectsPropertyStore::OpenUserPropertyStore`

and explains why the Creative Platform repository accesses per-context user FX properties rather than an arbitrary registry location.

## 5. amd64 COM registration

`DLL.AddReg.amd64` registers the following `CTUSBAPO64.dll` in-process COM classes:

| Class | CLSID |
|---|---|
| GFX | `{CA854A19-6601-407B-8AFB-CB5C2801AFE6}` |
| MFX | `{C624D7B2-8333-448E-85C8-51EEFC2025ED}` |
| LFX | `{DA3AD2CF-79F9-41B7-BE44-753ADEEC2EDD}` |
| SFX | `{71DAB6A1-39F3-423E-90A8-032729851157}` |
| EFX | `{EC2F4B76-6AE1-4DB9-8FF6-344B74CF9650}` |
| EffectNodeInfo | `{A14358D4-2952-4E26-8D27-8976993C4E61}` |

Every class above has `InProcServer32 = %SystemRoot%\System32\CTUSBAPO64.dll` on amd64.

Therefore the Creative DSP APO is an in-process dependency of the Windows audio graph.

## 6. ARM64 hosting conclusion

Microsoft documents custom APOs as in-process COM objects loaded by the Windows audio engine. Microsoft also documents Windows-on-Arm binary loading rules:

- an Arm64 process can load Arm64 binaries;
- Arm64 processes can also load Arm64X binaries because an Arm64X image contains an Arm64 view;
- plain x64 / Arm64EC binaries are not directly loadable by a classic Arm64 process.

The supplied `CTUSBAPO64.dll` is plain x86-64 PE32+, not ARM64, ARM64EC or ARM64X.

The supplied `ctusbaud.inf` likewise contains no ARM64 install target.

Consequently, the exact x64 Creative APO cannot be treated as an in-process native ARM64 AudioDG plugin merely because Windows 11 ARM can emulate x64 desktop applications.

This closes the earlier uncertainty: **the supplied x64 `CTUSBAPO64.dll` is not a viable direct in-process APO payload for native ARM64 AudioDG.**

## 7. Correct ARM64 architecture direction

For the first ARM64 effects restoration target:

1. keep the Microsoft USB Audio 2.0 base device path;
2. provide an ARM64-native APO package/extension using the same X4 endpoint FX bindings and Creative property schema;
3. implement native ARM64 SFX/MFX/EFX COM classes compatible with the required Windows APO interfaces;
4. bind Speaker/Headphone/Microphone first;
5. keep SPDIF/DDL separate because its official path additionally requires chainer/DGFX components;
6. preserve the previously recovered `IAudioSystemEffectsPropertyStore` controller/UI contract.

For current Windows 11 deployment, Microsoft documentation says APO deployment should use an `AudioProcessingObject`-class package/extension rather than simply copying the legacy Creative MEDIA INF packaging pattern.

Arm64X can be useful as a dual-architecture packaging technology only if an actual ARM64 implementation exists for the Arm64 view. It does not make the original x64 DSP code executable inside a native Arm64 audio-engine process by itself.

## 8. Scope consequences

Confirmed:

- X4 Speaker, Headphone and Microphone officially bind to Creative SFX/MFX/EFX.
- CrystalVoice on X4 is attached to the Microphone APO graph.
- The X4 Win11 INF contains the same Audio System Effects context/User/Default/Volatile structure used by the recovered Platform repository.
- The official x64 package is not ARM64-compatible as-is.
- Plain x64 `CTUSBAPO64.dll` cannot be the final native ARM64 in-process APO solution.
- SPDIF/DDL restoration is a separate later subproject because its official graph includes chainer + DGFX components.

Not yet solved:

- port/reimplementation of the actual Creative DSP algorithms into an ARM64 APO;
- which minimal subset of `CTUSBfilt64` semantics, if any, Speaker/Headphone/Mic effects require when the base class driver remains Microsoft `usbaudio2`;
- actual ARM64 APO runtime registration/build/validation;
- CDC Game/Voice raw UInt16 engineering-unit conversion.

## Safety

No new state-changing X4 command was issued by this trace.

Do not use this result as authorization for blind registry/endpoint writes on the live machine. First implementation should register an isolated ARM64 APO test package and validate discovery/read-only state before effect setters are enabled.

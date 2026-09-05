# DEBUG HISTORY — 2026-09-05 X4 ARM64 usbaudio2 Attachment Runtime Success

Branch:

`exp/windows-arm64-x4-native-controller`

## Scope

This record captures the read-only ARM64 runtime attachment probe for the Sound Blaster X4 USB Audio 2.0 interface.

No registry value was written. No endpoint property was changed. No CTCDC command was issued. No driver/APO package was installed.

Probe source:

`src/x4-usbaudio2-attachment-probe-arm64`

Runtime report:

`X4_USBAUDIO2_ATTACHMENT_REPORT.txt`

## 1. X4 audio devnode — confirmed

Present device instance:

`USB\VID_041E&PID_3278&MI_03\7&8197BA2&0&0003`

Properties:

- Class: `MEDIA`
- Service: `usbaudio2`
- DriverKey: `{4d36e96c-e325-11ce-bfc1-08002be10318}\0000`
- Friendly name: `Sound Blaster X4`

This proves the current Windows ARM64 machine is using Microsoft's USB Audio 2.0 function driver for the X4 audio interface.

## 2. Current registry attachment state — no Creative FX metadata

The probe read the X4 devnode hardware key, software/driver key, KSCATEGORY_AUDIO interface keys, and KSCATEGORY_TOPOLOGY interface key.

In every inspected location:

- `FX` subtree: not present
- `EP` subtree: not present

Therefore the current bare Microsoft `usbaudio2` state contains no installed Creative endpoint-FX metadata at those attachment locations.

This explains why the Stage A0 APO is not part of the live AudioDG graph even though the ARM64 APO DLL and COM classes themselves already pass isolated runtime validation.

Do not interpret this as an APO DLL failure.

## 3. Generated Microsoft interfaces — confirmed

KSCATEGORY_AUDIO exposes two X4 interfaces under the exact MI_03 devnode:

- `...#{6994ad04-93ef-11d0-a3cc-00a0c9223196}\msft_wave`
- `...#{6994ad04-93ef-11d0-a3cc-00a0c9223196}\msft_topo`

KSCATEGORY_TOPOLOGY exposes:

- `...#{dda54a40-1e4c-11d1-a050-405705c10000}\msft_topo`

All three interface devinst chains resolve directly to:

`USB\VID_041E&PID_3278&MI_03\7&8197BA2&0&0003`

## 4. KS topology — runtime map

The `msft_topo` KSCATEGORY_AUDIO interface exposes 9 pins.

Observed category map:

| Pin | Category | Data flow | Communication |
|---:|---|---|---:|
| 0 | `KSNODETYPE_SPEAKER` | OUT | 0 |
| 1 | `KSCATEGORY_AUDIO` / generic | IN | 0 |
| 2 | `KSNODETYPE_SPDIF_INTERFACE` | IN | 0 |
| 3 | `KSNODETYPE_MICROPHONE` | IN | 0 |
| 4 | `KSNODETYPE_LINE_CONNECTOR` | IN | 0 |
| 5 | `KSNODETYPE_SPDIF_INTERFACE` | OUT | 0 |
| 6 | `KSCATEGORY_AUDIO` / generic | IN | 0 |
| 7 | `KSCATEGORY_AUDIO` / generic | OUT | 0 |
| 8 | `KSNODETYPE_DIGITAL_AUDIO_INTERFACE` | IN | 0 |

This independently reproduces the same major endpoint-category families recovered from the official SB1815 INF:

- Speaker
- SPDIF
- Microphone
- Line
- Digital/What-U-Hear-style interface

The topology therefore survives under Microsoft's native ARM64 `usbaudio2` stack closely enough to support an X4-specific extension/APO attachment strategy.

## 5. Important unresolved point — Headphone association

No `KSNODETYPE_HEADPHONES` category appeared in the live KS topology dump.

The official Creative SB1815 Windows 11 INF nonetheless has a distinct `FX\1` Headphone APO binding using the same Creative SFX/MFX/EFX CLSIDs as Speaker.

Therefore do **not** equate `FX\1` with a live KS pin number and do not activate the review INF by assuming pin-index identity.

The exact runtime endpoint association for the Headphone endpoint still needs to be recovered from MMDevice / endpoint property metadata and compared with the official INF `PKEY_AudioEndpoint_Association` / `PKEY_FX_Association` data.

## 6. MMDevice endpoints — confirmed but association metadata not yet decoded

The read-only MMDevice scan found six X4-like endpoints (two render-class IDs and four capture-class IDs by endpoint-ID prefix).

Friendly-name text was partially mojibake in the redirected report, but all matched `Sound Blaster X4`.

`PKEY_Device_InstanceId` was empty in this enumeration path, so the current report does not yet map each MMDevice endpoint to a specific KS association category.

## 7. Packaging consequence

Confirmed safe conclusions:

1. The physical target devnode for an X4 extension remains `USB\VID_041E&PID_3278&MI_03`.
2. Microsoft's ARM64 `usbaudio2` generates `msft_wave` and `msft_topo` interfaces for that devnode.
3. Speaker/Mic/SPDIF/Line/Digital KS categories are present at runtime and align with the official Creative INF topology model.
4. No current `FX`/`EP` metadata is present, so the APO package must supply the missing endpoint FX metadata through the supported INF path rather than relying on existing Creative data.
5. Headphone association remains the blocking ambiguity before live FX binding.

## 8. Next action

Keep the package non-installing.

Next read-only step:

- dump all X4 MMDevice endpoint property keys/values;
- identify `PKEY_AudioEndpoint_Association`, form factor, physical-speaker/headphone differentiation, and any devnode/interface identity property;
- compare those values against the official SB1815 `PKEY_FX_Association` values;
- only then wire the `FX\0` / `FX\1` / `FX\3` payload into a live test extension INF.

## Safety

- no manual FX registry writes
- no live APO install yet
- no `regsvr32`
- no CTCDC writes
- no SPDIF/DDL work in this milestone
- no B5 ASIO changes

# SoundBlaster-X4-ARM64

Reverse-engineering research for controlling the **Creative Sound Blaster X4** on **Windows ARM64** without depending on the x64 Creative App.

## Scope

The first target is device control, not a replacement audio driver. The working assumption is that basic USB audio can remain on the Windows USB Audio class driver while X4-specific controls are reproduced separately where possible.

## Confirmed from the Android Creative App

The supplied Creative Android APK contains the following X4-specific identifiers:

- Product name: `Sound Blaster X4`
- Internal codename: `Accent2`
- Model identifier: `SB1815`
- BLE control name: `Control for SB1815`
- Device resource paths including `SB1815_metadata.json`, `SB1815_internal.json`, `SB1815_external.json`, and `SB1815_release.json`
- Android BLE/GATT classes and device configuration keys:
  - `keyServiceUUID`
  - `keyReadCharacteristicUUID`
  - `keyWriteCharacteristicUUID`

These facts confirm that the Android application contains X4-specific device-control logic and enough structure to continue tracing the BLE/GATT command path.

## Not Yet Confirmed

- Exact X4 GATT service UUID
- Exact X4 read/write characteristic UUIDs
- Command opcode layout
- Packet framing/checksum rules
- Which controls are BLE-only, USB-only, or implemented by on-device DSP state
- Windows ARM64 implementation architecture

## Current Milestone

**Identify the exact X4 GATT UUID set and trace one user-visible setting from UI action to transmitted command bytes.**

## Research Input

The original Creative APK is **not stored in this repository**.

SHA-256 of the APK used for the initial analysis:

`d613d4203585c4d50716ef0814b8b935906229930d280f2dc96bb6d6eb0479c1`

## Repository Layout

- `DEVICE_INFO.md` — confirmed device/application identifiers
- `RESEARCH_NOTES.md` — chronological research log
- `PROTOCOL.md` — protocol findings; confirmed and unresolved fields are kept separate
- `apk-analysis/` — sanitized APK analysis notes/scripts only
- `captures/` — sanitized protocol captures and annotations
- `tools/` — analysis utilities
- `src/` — future Windows ARM64 implementation

## Rule

Do not treat inferred values as confirmed. Protocol fields are promoted to **Confirmed** only after static-code evidence or runtime capture verification.

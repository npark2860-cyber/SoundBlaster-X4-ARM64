# SoundBlaster-X4-ARM64

Reverse-engineering research for controlling the **Creative Sound Blaster X4** on **Windows ARM64** without depending on the x64 Creative App.

## Scope

The first target is device control, not a replacement audio driver. Basic USB audio can remain on the Windows USB Audio class driver while X4-specific controls are reproduced separately where possible.

## Confirmed from the Android Creative App

- Product name: `Sound Blaster X4`
- Internal codename: `Accent2`
- Model identifier: `SB1815`
- BLE control name: `Control for SB1815`
- X4-specific resource paths for `SB1815`
- Android BLE/GATT transport and device-control logic

## Confirmed X4 BLE GATT Map

| Role | UUID |
|---|---|
| Service | `b7860001-11b8-b681-6343-5a6c2286633f` |
| Write characteristic | `b7860002-11b8-b681-6343-5a6c2286633f` |
| Read / notify characteristic | `b7860003-11b8-b681-6343-5a6c2286633f` |
| CCCD | `00002902-0000-1000-8000-00805f9b34fb` |

The direction of the two characteristics is confirmed from the app callback path: `b7860002` is used with `setValue()` + `writeCharacteristic()`, while `b7860003` is registered for notifications and matched by `onCharacteristicChanged()`.

See `apk-analysis/GATT_UUID_TRACE_20260831.md` for the static trace.

## Not Yet Confirmed

- Command opcode / command-ID layout
- Packet framing and length fields
- Checksum / CRC behavior
- Exact command bytes for individual X4 settings
- Which controls are BLE-only, USB-only, or implemented by on-device DSP state
- Final Windows ARM64 implementation architecture

## Current Milestone

**Trace one simple X4 user-visible setting from UI action to the exact command bytes written to the BLE write characteristic.**

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

Do not treat inferred values as confirmed. Protocol fields are promoted to **Confirmed** only after static-code evidence, runtime capture, or independent reproduction.

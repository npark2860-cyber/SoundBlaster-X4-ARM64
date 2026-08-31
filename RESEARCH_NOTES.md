# Research Notes

## 2026-08-31 — Initial APK static triage

### Input

Android Creative App APK supplied for analysis.

SHA-256:

`d613d4203585c4d50716ef0814b8b935906229930d280f2dc96bb6d6eb0479c1`

### Confirmed findings

1. The application contains direct Sound Blaster X4 identifiers:
   - `Sound Blaster X4`
   - `Accent2`
   - `SB1815`
   - `Control for SB1815`

2. X4/SB1815-specific resource names are embedded in the APK:
   - `/SB1815_metadata.json`
   - `SB1815/SB1815_external.json`
   - `SB1815/SB1815_internal.json`
   - `SB1815/SB1815_release.json`
   - `SB1815_equalizer`

3. Android BLE/GATT APIs and device-config keys are present:
   - `BluetoothGatt`
   - `BluetoothGattCharacteristic`
   - `BluetoothGattService`
   - `keyServiceUUID`
   - `keyReadCharacteristicUUID`
   - `keyWriteCharacteristicUUID`

4. The APK contains ARM64 native libraries in `lib/arm64-v8a/`.

### Interpretation

The Android Creative App contains X4-specific control logic rather than only generic UI metadata. Static tracing should therefore be sufficient to identify the X4 GATT configuration and command construction path before runtime capture is required.

### Current next action

Trace `Accent2` / `SB1815` into the device configuration that supplies:

- service UUID
- read characteristic UUID
- write characteristic UUID

Then trace one simple user-visible control through to the final command `byte[]` passed to the BLE write path.

### Do not assume yet

- Any candidate UUID is X4-specific until its `Accent2`/`SB1815` association is proven.
- Any command byte layout is stable until verified by code path or runtime capture.
- BLE control coverage is identical to the Windows Creative App.

# Device Information

## Target

- Product: Creative Sound Blaster X4
- Internal codename found in Android Creative App: `Accent2`
- Model identifier found in Android Creative App: `SB1815`
- BLE control name found in Android Creative App: `Control for SB1815`

## Initial Analysis Artifact

- File: `Creative.apk` (not committed)
- Size: `45,222,154` bytes
- SHA-256: `d613d4203585c4d50716ef0814b8b935906229930d280f2dc96bb6d6eb0479c1`
- DEX files observed: `classes.dex`, `classes2.dex`
- ARM64 native libraries are present under `lib/arm64-v8a/`

## X4-specific strings confirmed in `classes.dex`

- `Sound Blaster X4`
- `Accent2`
- `SB1815`
- `Control for SB1815`
- `/SB1815_metadata.json`
- `SB1815/SB1815_external.json`
- `SB1815/SB1815_internal.json`
- `SB1815/SB1815_release.json`
- `SB1815_equalizer`
- `accent2_`

## BLE/GATT-related strings confirmed

- `android/bluetooth/BluetoothGatt`
- `android/bluetooth/BluetoothGattCallback`
- `android/bluetooth/BluetoothGattCharacteristic`
- `android/bluetooth/BluetoothGattDescriptor`
- `android/bluetooth/BluetoothGattService`
- `keyServiceUUID`
- `keyReadCharacteristicUUID`
- `keyWriteCharacteristicUUID`

## Unresolved

The exact UUID values and command bytes used specifically by X4 have not yet been promoted to confirmed status.

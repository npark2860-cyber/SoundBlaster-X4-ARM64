# Protocol Notes

This file separates **confirmed protocol facts** from unresolved command-layer work.

## Confirmed X4 Identity

| Item | Value | Status |
|---|---|---|
| Product | `Sound Blaster X4` | Confirmed |
| Internal codename | `Accent2` | Confirmed |
| Model identifier | `SB1815` | Confirmed |
| BLE control device name | `Control for SB1815` | Confirmed |

## Confirmed X4 GATT Map

| Field | Value | Status |
|---|---|---|
| Service UUID | `b7860001-11b8-b681-6343-5a6c2286633f` | Confirmed |
| Write characteristic UUID | `b7860002-11b8-b681-6343-5a6c2286633f` | Confirmed |
| Read / notify characteristic UUID | `b7860003-11b8-b681-6343-5a6c2286633f` | Confirmed |
| Client Characteristic Configuration Descriptor | `00002902-0000-1000-8000-00805f9b34fb` | Confirmed |

### Static trace

The Android connection path calls an obfuscated selector equivalent to:

`connect(address, bleName)` → select UUID set from `bleName` → `BluetoothDevice.connectGatt(...)`.

The selector has explicit alternate UUID branches for device-name families such as `MF8345`, `MF8380`, `MF8400`, `MF8415`, `SB1820`, and several `EF*` / `MF*` products. `Control for SB1815` does not match any alternate branch, so it takes the default UUID set:

- service = `b7860001-...`
- characteristic B = `b7860002-...`
- characteristic C = `b7860003-...`
- descriptor = `00002902-...`

Direction is established separately by callback behavior:

- characteristic B is passed to `setValue(...)` and `BluetoothGatt.writeCharacteristic(...)` → **write**
- characteristic C is registered with `setCharacteristicNotification(...)` and matched in `onCharacteristicChanged(...)` → **read / notify**

### Independent corroboration

Public Windows device-enumeration records containing `Control for SB1815` also enumerate the custom BLE service `b7860001-11b8-b681-6343-5a6c2286633f` on the same system. This corroborates the static APK trace.

## Transport Behavior Confirmed

The application ultimately transmits command bytes with:

`byte[]` → `BluetoothGattCharacteristic.setValue(...)` → `BluetoothGatt.writeCharacteristic(...)`.

Notifications are enabled through the standard CCCD using `ENABLE_NOTIFICATION_VALUE`.

## Command Format

Not yet resolved.

| Field | Offset | Size | Status |
|---|---:|---:|---|
| Header | — | — | Unresolved |
| Opcode / command ID | — | — | Unresolved |
| Payload | — | — | Unresolved |
| Length | — | — | Unresolved |
| Checksum/CRC | — | — | Unresolved |

## Current Next Target

Trace one simple X4 user-visible control from UI action to the exact `byte[]` written to `b7860002-11b8-b681-6343-5a6c2286633f`.

## Validation Rule

A protocol value becomes **Confirmed** only when supported by a static code path, runtime capture, or independent reproduction. Inferred values remain explicitly labeled until promoted by evidence.

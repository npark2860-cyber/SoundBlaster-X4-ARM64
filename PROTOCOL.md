# Protocol Notes

This file separates **confirmed protocol facts** from candidates and hypotheses.

## Confirmed

| Item | Status | Evidence |
|---|---|---|
| X4 Android identifier | Confirmed | `Sound Blaster X4` string in APK |
| Internal codename | Confirmed | `Accent2` string in APK |
| Model identifier | Confirmed | `SB1815` string in APK |
| BLE control device name | Confirmed | `Control for SB1815` string in APK |
| BLE/GATT transport code exists | Confirmed | Android `BluetoothGatt*` classes in APK |
| Device configuration includes service UUID field | Confirmed | `keyServiceUUID` |
| Device configuration includes read characteristic UUID field | Confirmed | `keyReadCharacteristicUUID` |
| Device configuration includes write characteristic UUID field | Confirmed | `keyWriteCharacteristicUUID` |

## Unresolved X4 GATT Map

| Field | Value | Status |
|---|---|---|
| Service UUID | — | Unresolved |
| Read characteristic UUID | — | Unresolved |
| Write characteristic UUID | — | Unresolved |
| Notification/indication behavior | — | Unresolved |
| MTU requirements | — | Unresolved |

## Command Format

Not yet resolved.

| Field | Offset | Size | Status |
|---|---:|---:|---|
| Header | — | — | Unresolved |
| Opcode | — | — | Unresolved |
| Payload | — | — | Unresolved |
| Length | — | — | Unresolved |
| Checksum/CRC | — | — | Unresolved |

## Validation Rule

A protocol value becomes **Confirmed** only when at least one of these is available:

1. Static code/resource evidence that directly associates the value with `Accent2` or `SB1815`.
2. Runtime BLE capture showing the X4 using that value.
3. Independent reproduction where a manually generated command produces the expected X4 state change.

Candidates must remain explicitly labeled as candidates until one of those conditions is met.

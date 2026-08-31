# Protocol Notes

This file separates **confirmed protocol facts** from runtime-dependent protocol state.

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
| CCCD | `00002902-0000-1000-8000-00805f9b34fb` | Confirmed |

The Android connection path selects the default Creative B786 UUID family for `Control for SB1815`. `b7860002` is used with `setValue()` + `writeCharacteristic()`, while `b7860003` is registered for notifications and matched in `onCharacteristicChanged()`.

## Confirmed Command Wrapper

Commands are built as:

`feature payload` → `fi/i.W(payload, command, chipset)` → `md/a.i(bytes)` → BLE write characteristic.

For the normal X4 path the chipset argument is `MIDAS`.

### Legacy MIDAS framing

When the runtime extended-frame flag is false:

`6A <command> <length_lo> <length_hi> <payload...>`

MIDAS does not add a second envelope or escaping after this concatenation.

### Extended MIDAS framing

A runtime protocol flag can switch the wrapper to an extended frame. The flag starts false and can be enabled by protocol negotiation response handling.

Extended frames begin with:

`5C <command> <length_lo> <length_hi> <payload...>`

and append three runtime bytes containing sequence/protocol state and CRC information. These suffix bytes are session-dependent and must be generated, not hard-coded.

## First Traced Control — Direct Mode

The Direct Mode UI reaches:

`qg/i2.f(DIRECT_MODE, checked)`

`DIRECT_MODE.getIndex()` is confirmed as `5` (`0x05`).

The command builder creates:

| State | Raw payload |
|---|---|
| OFF | `00 05 00` |
| ON | `00 05 01` |

The wrapper call is:

`fi/i.W(payload, 57, MIDAS)`

so the command ID is `57` decimal = `0x39`.

### Exact legacy-mode writes

| State | Final BLE write bytes |
|---|---|
| OFF | `6A 39 03 00 00 05 00` |
| ON | `6A 39 03 00 00 05 01` |

These bytes are final for the legacy framing state; `md/a.i()` does not further transform them before the normal X4 BLE write path.

See `apk-analysis/DIRECT_MODE_TRACE_20260831.md` for the full static trace.

## Still Requires Runtime Validation

The APK alone cannot tell us which negotiated framing state a particular X4 firmware session ultimately uses. One runtime capture of a Direct Mode toggle is sufficient to resolve this.

Validation target:

- characteristic: `b7860002-11b8-b681-6343-5a6c2286633f`
- action: Direct Mode OFF→ON or ON→OFF
- expected legacy frames: the two seven-byte frames above
- otherwise: capture the `5C` extended frame and reproduce its sequence/CRC generation

## Validation Rule

A device-runtime claim is promoted to **Confirmed** only after static-code evidence plus either a runtime capture or independent reproduction on the X4.

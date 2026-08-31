# X4 Direct Mode Static Trace — 2026-08-31

APK SHA-256:

`d613d4203585c4d50716ef0814b8b935906229930d280f2dc96bb6d6eb0479c1`

## UI to command builder

The Direct Mode switch listener reaches:

`qg/i2.f(DIRECT_MODE, checked)`

`DIRECT_MODE` is constructed with feature index `5`, and `getIndex()` returns that stored index.

The feature setter converts the switch value and index to bytes and builds exactly three payload bytes:

| Byte | Meaning |
|---:|---|
| `0x00` | feature-set operation prefix |
| `0x05` | `DIRECT_MODE` feature index |
| `0x00` / `0x01` | OFF / ON |

Therefore the pre-wrapper payloads are confirmed:

- OFF: `00 05 00`
- ON: `00 05 01`

The payload is passed to:

`fi/i.W(payload, 57, MIDAS)`

Decimal `57` is command `0x39`.

## MIDAS legacy frame

When the negotiated extended-frame flag is false, the MIDAS wrapper calls the header builder equivalent to:

`[0x6A, command, length_lo, length_hi]`

MIDAS then concatenates the header and payload without a second envelope or byte escaping.

For Direct Mode this gives:

- OFF: `6A 39 03 00 00 05 00`
- ON: `6A 39 03 00 00 05 01`

These are the exact final bytes for the legacy MIDAS framing state.

## Negotiated extended frame

The wrapper has a runtime global protocol flag. It starts false, but the response parser for command `0x0F` can promote it to true when the returned protocol payload reports marker `0xFF`.

When true, the command is not a fixed seven-byte frame. The wrapper uses:

- base header `5C 39 03 00`
- payload `00 05 00/01`
- a three-byte runtime suffix containing sequence/protocol state and a lookup-table CRC

So the exact transmitted bytes in this mode depend on live session state.

Do **not** hard-code one extended-mode frame from static analysis.

## X4 transport-path exclusions

Other conditionals inside the wrapper belong to alternate product/transport paths:

- `Lbr/a;.c` is associated with the Aurvana/Auria connection path.
- `Lp3/a;.c` is associated with a separate Actions/IBluz connection path.
- X4's confirmed BLE path is the `Lxc/a;` path selected as connection type `0`.
- product-name exception flags in `Lhd/b;` are populated from lists that do not include `Control for SB1815`.

These branches therefore do not define the normal X4 BLE framing choice. The remaining runtime-dependent distinction is the negotiated legacy/extended protocol-frame flag.

## Final send path

The result returned by `fi/i.W(...)` is passed through `md/a.i(...)` to the X4 BLE queue. The queue sends the same byte array using:

`BluetoothGattCharacteristic.setValue(bytes)`

followed by:

`BluetoothGatt.writeCharacteristic(...)`

There is no additional command-byte transformation after `fi/i.W(...)` on the normal X4 BLE path.

## Runtime validation target

Capture one Direct Mode OFF→ON or ON→OFF operation on the actual X4. A single write to characteristic `b7860002-11b8-b681-6343-5a6c2286633f` will determine which negotiated framing mode the X4 firmware is using and will validate the full command chain.

# Protocol Notes

This file separates confirmed X4 protocol facts from unresolved areas.

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

`b7860002` is the X4 BLE write characteristic. `b7860003` is used for notifications/readback.

## Confirmed Direct Mode Command

Static analysis identifies:

- command ID: `0x39`
- feature index: `0x05`
- OFF payload: `00 05 00`
- ON payload: `00 05 01`

Reinspection of the MIDAS legacy frame builder plus physical-X4 testing establishes the actual frame format used for this control:

`5A <command> <payload_length> <payload...>`

For Direct Mode:

| State | Final raw BLE write | Runtime result |
|---|---|---|
| OFF | `5A 39 03 00 05 00` | Direct Mode turned OFF |
| ON | `5A 39 03 00 05 01` | Direct Mode turned ON |

Both commands were entered into the hidden `DebugProtocolFragment`, which converts the hex string to bytes and writes those bytes directly to the X4 BLE path. Both state transitions were observed on the physical Sound Blaster X4.

Therefore the Direct Mode command is **runtime confirmed end-to-end**.

## Correction of Earlier Interpretation

Earlier notes incorrectly decoded the legacy MIDAS header as `6A` with a two-byte length field and therefore proposed:

- `6A390300000500`
- `6A390300000501`

The physical X4 did not react to the OFF frame. Reinspection showed the correct legacy header is `5A` with a one-byte payload length. The obsolete `6A` frames must not be used.

A trial `5C` extended-frame candidate also produced no observable state change. That test is not evidence that Direct Mode requires extended framing; the confirmed X4 Direct Mode path is the `5A` frame above.

## Debug Protocol Default Command

The APK-hardcoded Debug Protocol default is:

`FF040004000A00C06A030000`

Sending it produced a Direct Mode ON notification on the physical X4, but its field semantics are not yet resolved. It is not the canonical Direct Mode setter now that the exact `5A 39 03 00 05 00/01` pair has been independently reproduced.

## Next Milestone

Implement the smallest Windows ARM64 BLE proof-of-concept that:

1. connects to `Control for SB1815`;
2. discovers the confirmed B786 service/characteristics;
3. writes `5A3903000500` and `5A3903000501`;
4. reproduces Direct Mode OFF/ON without Creative App.

Do not start a full replacement UI until this independent Windows ARM64 reproduction succeeds.

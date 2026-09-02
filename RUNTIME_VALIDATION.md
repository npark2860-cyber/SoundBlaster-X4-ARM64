# Runtime Validation — Sound Blaster X4

## Status

**PASSED — Direct Mode OFF/ON reproduced on physical X4 using raw BLE command bytes.**

## Confirmed Transport

- BLE device name: `Control for SB1815`
- Service: `b7860001-11b8-b681-6343-5a6c2286633f`
- Write characteristic: `b7860002-11b8-b681-6343-5a6c2286633f`
- Notify/read characteristic: `b7860003-11b8-b681-6343-5a6c2286633f`
- CCCD: `00002902-0000-1000-8000-00805f9b34fb`

## Confirmed Direct Mode Commands

Command ID: `0x39`

Feature index: `0x05`

| State | Raw command | Physical-X4 result |
|---|---|---|
| OFF | `5A3903000500` | Direct Mode OFF |
| ON | `5A3903000501` | Direct Mode ON |

Frame layout:

`5A <command> <payload_length> <payload...>`

Direct Mode payload length is `03`, with payload `00 05 00/01`.

## Validation Method

The existing hidden Creative Android `DebugProtocolFragment` was exposed by a minimal APK patch. The fragment is a raw hexadecimal sender: entered hex is converted to bytes and passed directly into the X4 BLE write path.

Physical-device sequence:

1. Send `5A3903000500`.
2. X4 Direct Mode turns OFF.
3. Send `5A3903000501`.
4. X4 Direct Mode turns ON.

This confirms the command format and state byte independently of the normal Direct Mode UI.

## Corrected Earlier Failure

Earlier proposed `6A390300000500/01` frames were based on a static decoding error and are obsolete. The OFF frame produced no observable response on the X4. A trial `5C` candidate also produced no state change.

The confirmed Direct Mode command uses `5A`, a one-byte payload length, and no extra zero length byte.

## Next Action

Build a minimal Windows ARM64 proof-of-concept with only these responsibilities:

1. discover/connect to `Control for SB1815`;
2. resolve service `b7860001-11b8-b681-6343-5a6c2286633f`;
3. resolve write characteristic `b7860002-11b8-b681-6343-5a6c2286633f`;
4. send the confirmed OFF/ON six-byte frames;
5. verify the X4 changes state.

A Bluetooth HCI capture is no longer required for the first proof-of-concept.

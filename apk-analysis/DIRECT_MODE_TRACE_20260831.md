# X4 Direct Mode Trace — corrected and runtime validated

APK SHA-256:

`d613d4203585c4d50716ef0814b8b935906229930d280f2dc96bb6d6eb0479c1`

## UI to command builder

The Direct Mode switch listener reaches:

`qg/i2.f(DIRECT_MODE, checked)`

`DIRECT_MODE.getIndex()` is `5` (`0x05`). The feature setter builds three payload bytes:

- OFF: `00 05 00`
- ON: `00 05 01`

The payload is passed to:

`fi/i.W(payload, 57, MIDAS)`

Decimal `57` is command `0x39`.

## Correct MIDAS legacy frame

A previous static reading incorrectly identified the legacy header as `6A` plus a two-byte length. Reinspection and hardware validation corrected this.

For the X4 Direct Mode path the raw frame is:

`5A <command> <payload_length> <payload...>`

Therefore:

- OFF: `5A 39 03 00 05 00`
- ON: `5A 39 03 00 05 01`

## Physical X4 validation

The hidden Creative Android `DebugProtocolFragment` is a raw hex sender. It converts entered hexadecimal text to a byte array and sends the same bytes to the X4 BLE write path.

On the physical Sound Blaster X4:

- `5A3903000500` produced Direct Mode OFF.
- `5A3903000501` produced Direct Mode ON.

This validates the complete Direct Mode command end-to-end.

## Rejected earlier candidates

The following are obsolete and must not be used:

- `6A390300000500` — no observable response; based on incorrect header/length decoding.
- `6A390300000501` — same incorrect format.
- tested `5C` extended Direct Mode candidate — no observable state change.

The existence of a `5C` protocol branch elsewhere in the APK remains a separate protocol-analysis topic. It is not required for the confirmed Direct Mode command on the tested X4.

## Final send path

The normal X4 BLE write target remains:

`b7860002-11b8-b681-6343-5a6c2286633f`

No additional framing is required around the confirmed six-byte Direct Mode commands when reproducing them through the raw sender.

## Status

**Direct Mode static trace + physical-device reproduction: COMPLETE.**

Next target: reproduce these writes from a minimal Windows ARM64 BLE client.

# X4 Runtime Protocol State Diagnostic — 2026-08-31

## Runtime result

On a real Sound Blaster X4, sending the statically derived legacy MIDAS Direct Mode OFF frame

`6A390300000500`

through the patched Android Creative App Debug Protocol raw sender produced **no observable response/state change**.

This is direct runtime evidence that the current X4 session is not accepting the legacy `6A` frame for this command path.

## Important transport clarification

`DebugProtocolFragment` sends entered bytes through `md/a.j(byte[])`.

For the active X4 BLE path this reaches `hd/a.h(byte[])`, which writes the same supplied byte array to the X4 BLE write characteristic. No command framing is added by the Debug Protocol path.

Therefore the failed `6A390300000500` test is a valid raw-wire negative result, not a UI parsing failure.

## Extended MIDAS state

`fi/i.W(...)` has a runtime extended-frame branch controlled by:

- `Loi/e;.d` — extended frame enabled flag
- `Loi/e;.a` — sequence counter A
- `Loi/e;.c` — protocol/session state value

The extended frame starts with:

`5C <command> <payload_length_lo> <payload_length_hi> ...`

and appends runtime sequence/state bytes plus a lookup-table CRC.

For Direct Mode the logical command remains:

- command: `0x39`
- OFF payload: `00 05 00`
- ON payload: `00 05 01`

but the exact raw frame cannot be hard-coded without the live `d/a/c` values.

## Diagnostic APK patch

A diagnostic build was produced that keeps the existing Dashboard module-ID patch used to expose `DebugProtocolFragment` and replaces the hardcoded default Debug Protocol input with one decimal integer encoding the live protocol state:

`state = (extendedFlag << 16) | (sequenceA << 8) | protocolC`

This allows all state required to calculate the next exact extended frame to be recovered from a single screen read without multiple trial packets.

Diagnostic APK SHA-256:

`b5bc1bc37a5b18eeffcaa091554d7a389f9e8d55fbc767fdea28d67545ee796d`

The diagnostic APK is signed with the same local test key as the previously supplied Debug Protocol patch, so it can be installed as an update over that patched build.

# X4 Runtime Validation — Debug Protocol

## Device / app

- Device: Sound Blaster X4 (`SB1815` / `Accent2`)
- Android Creative App: 2.11.08 Internal Beta
- Test path: patched Dashboard entry -> existing `DebugProtocolFragment`

## Raw sender confirmed

`DebugProtocolFragment` converts the entered hexadecimal string into bytes and sends those bytes through the X4 BLE write path without adding command framing.

Its APK-hardcoded initial command is:

`FF040004000A00C06A030000`

Sending that value produced a user-visible Direct Mode ON notification on the physical X4. Its internal field semantics remain unresolved and it is not used as the canonical Direct Mode setter.

## Direct Mode experiments

### Rejected static candidates

`6A390300000500`

- result: no observable response/state change
- cause: later reinspection showed this frame was built from an incorrect legacy-header/length interpretation

A calculated `5C` extended-frame candidate was also tested and produced no observable response/state change. It is not the active Direct Mode format on the tested X4.

### Corrected and confirmed commands

`5A3903000500`

- result: **Direct Mode OFF**

`5A3903000501`

- result: **Direct Mode ON**

This establishes the runtime-confirmed Direct Mode frame:

`5A 39 03 00 05 <state>`

where:

- `0x39` = command ID
- `0x03` = payload length
- `0x00 0x05` = set feature / Direct Mode selector
- state `0x00` = OFF
- state `0x01` = ON

## Final status

Direct Mode OFF/ON is now independently reproducible on the physical X4 through raw BLE command bytes.

The next validation boundary is no longer Android. It is a minimal Windows ARM64 BLE client sending the same confirmed six-byte commands to characteristic `b7860002-11b8-b681-6343-5a6c2286633f`.

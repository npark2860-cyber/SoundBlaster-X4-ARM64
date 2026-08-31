# X4 Runtime Validation — Debug Protocol — 2026-08-31

## Device / app

- Device: Sound Blaster X4 (`SB1815` / `Accent2`)
- Android Creative App: 2.11.08 Internal Beta
- Test path: patched Dashboard entry -> existing `DebugProtocolFragment`

## Confirmed runtime observation

`DebugProtocolFragment` is a working raw hexadecimal sender.

The fragment's APK-hardcoded initial command is:

`FF040004000A00C06A030000`

On the physical X4, pressing **SEND** with this exact value produced the user-visible state notification that Direct Mode was turned on.

Therefore the following is runtime-confirmed:

- the hidden Debug Protocol screen can transmit to the X4;
- the hardcoded command reaches and is accepted by the physical device;
- sending `FF040004000A00C06A030000` results in an observed Direct Mode ON state/notification on this X4.

## Important distinction

Do not equate the hardcoded Debug Protocol test command with the normal Direct Mode setter yet.

Static tracing of the normal Direct Mode switch independently shows:

- feature index: `0x05`
- command ID: `0x39`
- OFF payload: `00 05 00`
- ON payload: `00 05 01`
- legacy MIDAS frames:
  - OFF: `6A390300000500`
  - ON: `6A390300000501`

The Debug Protocol default command instead contains:

`FF040004000A00C06A030000`

Its exact semantic/framing relationship to the normal MIDAS Direct Mode setter is not yet resolved. The observed Direct Mode effect is confirmed; the internal meaning of each field is not.

## Next discriminating runtime test

Send the statically derived normal legacy Direct Mode OFF frame through Debug Protocol:

`6A390300000500`

If the physical X4 turns Direct Mode off, the active normal X4 control path accepts legacy MIDAS framing directly. Then send `6A390300000501` to verify ON.

If it does not react, do not vary bytes blindly; investigate the negotiated/outer framing path before further tests.

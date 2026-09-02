# X4 Runtime Protocol Diagnostic — superseded interpretation

## Historical result

A diagnostic build was created after the incorrectly decoded frame `6A390300000500` produced no observable response on the physical X4.

The diagnostic build attempted to encode runtime protocol state as:

`state = (extendedFlag << 16) | (sequenceA << 8) | protocolC`

The displayed value was `-256` because signed byte extension was not masked in the diagnostic packing code. That diagnostic output was therefore not a reliable single-integer representation of all three fields.

## Correction

The `6A` failure was later explained by a static decoding error, not by proof that Direct Mode required an extended `5C` frame.

Reinspection and direct physical-X4 testing established the actual Direct Mode commands:

- OFF: `5A3903000500`
- ON: `5A3903000501`

Both commands produced the expected state changes.

A separately calculated `5C` candidate produced no observable state change and is not required for the confirmed Direct Mode path.

## Status

The protocol-state diagnostic is no longer needed for Direct Mode and should not be used to choose between `6A` and `5C` framing for that control.

Authoritative Direct Mode protocol information is in:

- `PROTOCOL.md`
- `apk-analysis/DIRECT_MODE_TRACE_20260831.md`
- `captures/RUNTIME_DIRECT_MODE_20260831.md`

Diagnostic APK SHA-256 retained for historical traceability:

`b5bc1bc37a5b18eeffcaa091554d7a389f9e8d55fbc767fdea28d67545ee796d`

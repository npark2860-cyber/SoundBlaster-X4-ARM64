# Debug Protocol Entry Trace — 2026-08-31

## Result

The APK contains a fully implemented raw protocol sender, `DebugProtocolFragment`, but the normal dashboard module list in this build does **not** instantiate the module ID required to navigate to it.

## Raw sender

Class:

`com.creative.apps.creative.ui.device.module.debugprotocol.DebugProtocolFragment`

The fragment accepts a hexadecimal string, converts pairs of hex characters into a `byte[]`, and submits the resulting bytes to the device command transport (`Lmd/a;->j([B)`).

A default command string is inserted by the fragment:

`FF040004000A00C06A030000`

Resource/navigation entries include:

- `fragment_debug_protocol`
- `button_debug_protocol_send`
- `textView_debug_protocol_command_sent`
- `textView_debug_protocol_command_received`
- navigation destination label: `Debug Protocol`
- action: `action_dashboardFragment_to_debugProtocolFragment`

## Dashboard dispatch

The dashboard click dispatcher reads `Lj9/m;->a:I` as the module ID.

When the module ID equals decimal `100` (`0x64`), it executes:

`action_dashboardFragment_to_debugProtocolFragment`

Therefore:

`dashboard module ID 100 -> DebugProtocolFragment`

## Feature key

`Lf9/p0;->t(I)` maps module ID `100` to:

`show_debug_protocol`

The generic module-visibility preference reader can therefore handle ID 100 if such a module exists.

## Reachability check

All constructor call sites for the dashboard item class `Lj9/m` were enumerated across the APK DEX files.

The only construction paths are:

1. `Lf9/p0;->s(String)` — normal dashboard module-list builder
2. `DashboardFragment->O(...)` — two special runtime dashboard additions

The normal builder constructs module IDs in the ordinary range (observed IDs include 1–5, 12–16, 18–23, and 25). It never constructs ID 100.

The two runtime additions in `DashboardFragment->O(...)` also use ordinary module IDs, not 100.

`Lj9/m;->a` is assigned by the constructor and no separate mutation path was found that changes an existing dashboard item into ID 100.

The only call sites of the generic feature-visibility reader `Lf9/p0;->r(p0, id)` are the normal builder and the two special dashboard additions. No independent remote/runtime path was found that injects module ID 100.

## Conclusion

In Creative Android 2.11.08 Internal Beta, the raw Debug Protocol screen and its navigation case remain compiled into the APK, but the standard dashboard construction code does not create the ID-100 entry needed to reach it.

`show_debug_protocol` is therefore a surviving feature key, not by itself an accessible UI switch in this build.

The Account/About hidden debug mode is separate. Its `Session Id` / `Session Auth Code` page is the Realtime Logging server-authentication flow and does not navigate to `DebugProtocolFragment`.

## Practical next routes

To use the built-in raw sender, one of these would be required:

- patch the APK so an existing dashboard item or a newly added ID-100 item navigates to the existing Debug Protocol destination; or
- bypass the Creative UI and send directly to the confirmed X4 BLE write characteristic with a separate test client.

No APK modification was performed during this trace.

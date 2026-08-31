# Debug UI Trace — 2026-08-31

## Important distinction

The screen containing `Session Id` and `Session Auth Code` is **not** the X4 raw protocol console.

It is the app's realtime analytics authentication UI (`view_analytic_authentication`). The About screen reads both fields and sends them through `RealtimeLogAuthenticationQueryModel`; authentication is handled by the Creative realtime-log backend and successful authentication returns a session token.

There is no hard-coded session ID/auth code in the APK path inspected here.

## Actual raw protocol console

The actual X4 command console is `DebugProtocolFragment` / navigation label `Debug Protocol`.

Its layout contains:

- `Command to send (in hex):`
- `SEND`
- `Response:`

The edit field is pre-populated by the app with:

`FF040004000A00C06A030000`

When SEND is pressed the app parses the hexadecimal string into `byte[]` and forwards it to the device write path.

Navigation contains `action_dashboardFragment_to_debugProtocolFragment`.

The internal module enum is:

- display name: `Debug CT Protocol`
- module ID: `DEBUG_PROTOCOL`
- metadata key: `show_debug_protocol`

## Practical consequence

Do not spend time trying to recover or guess the Session Id / Session Auth Code. That UI is for Creative realtime analytics logging, not for BLE command injection.

The next runtime target is the dashboard's `Debug Protocol` page, then send one known-safe X4 command and observe the response/device state.
# Internal Beta Authentication Trace — 2026-08-31

## Correction

The `Session Id` / `Session Auth Code` screen shown after enabling the hidden Internal Beta mode is **not** the raw X4 protocol sender.

The values entered in this screen are used for Creative's realtime logging authentication flow.

## Static APK trace

The About/Internal Beta UI reads the two text fields and, when both are non-empty, launches a coroutine that reaches:

`gh/l.h(String sessionId, String sessionAuthCode, ...)`

That method constructs:

`RealtimeLogAuthenticationQueryModel(sessionId, sessionAuthCode)`

and sends it through the application's network layer.

On a successful server response, the app extracts a `sessionToken`, stores it, logs `Authentication Success`, and shows the corresponding success Toast.

On a failed server response, the app clears the stored token and shows `Authentication Failed: ...`.

## Consequence

Protocol values such as:

- command ID `0x39`
- Direct Mode payload `00 05 01`

must **not** be entered into this authentication form. They belong to the local X4 BLE command path, not the Creative realtime-log server authentication path.

## Next action

Do not spend time guessing Session Id/Auth Code values. Continue X4 protocol validation through the local BLE path (`b7860001` service / `b7860002` write characteristic) or locate the actual Debug Protocol sender UI separately.

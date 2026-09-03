# DEBUG HISTORY — ASIO COM Stage B2 BUSY runtime success

Date: 2026-09-04 KST

## Source under test

Branch:

`exp/windows-arm64-asio-com-stage-b2-wavert`

Validated source HEAD at test time:

`a6d3201260056a46ae8bce57271132871904d6ee`

Executable:

`x4-asio-stage-b2-smoke.exe`

## Runtime result

The user ran the Stage B2 registry-free COM/WaveRT smoke while the X4 Render Pin 1 global instance was already occupied.

Observed output:

```text
init=0
initMessage=Stage B2 init preflight BUSY: C 0/1 G 1/1; KsCreatePin SKIPPED
driverName=Sound Blaster X4 ARM64
driverVersion=102
createBuffers/start/stop=SKIPPED because init did not report FREE
DllCanUnloadNow hr=0x00000000
STAGE B2 COM/WAVERT RESULT: PASS (BUSY SAFELY BLOCKED AT INIT)
```

## Hardware-established conclusions

- Render Pin 1 local instance count remained `0/1`.
- Render Pin 1 global instance count was saturated at `1/1`.
- Stage B2 COM `init()` returned false before any WaveRT preparation.
- `createBuffers()` was not called.
- the real render `KsCreatePin` path was not reached.
- `start()` and `stop()` were not called.
- COM objects unloaded cleanly.
- no green-screen / WDF crash occurred.

This confirms that adding the fixed WaveRT engine to the Stage B2 DLL did not regress the already-proven B1 coexistence refusal path.

## What this does NOT prove yet

This result does not validate the B2 FREE path because the real render pin was not instantiated.

Still unverified for B2:

- second CINSTANCES/GLOBALCINSTANCES gate immediately before real `KsCreatePin`
- successful Render Pin 1 creation through the COM object
- 4096-byte WaveRT notification buffer acquisition
- notification event registration
- `ACQUIRE -> PAUSE -> RUN`
- 20/20 notifications
- packet continuity / presentation-position monotonicity
- `RUN -> PAUSE -> ACQUIRE -> STOP`
- notification unregister and clean pin/filter close

## Immediate next action

Do not change code.

Ensure all Windows playback using X4 is stopped and rerun the exact same Stage B2 smoke once.

Required initial state:

```text
init=1
```

Only that FREE run may proceed into the fixed WaveRT lifecycle.

Do not intentionally bypass a BUSY result and do not use the old ungated baseline while Windows playback is active.

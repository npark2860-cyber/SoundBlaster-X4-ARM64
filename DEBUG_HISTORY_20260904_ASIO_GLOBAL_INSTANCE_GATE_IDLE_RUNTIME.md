# DEBUG HISTORY — ARM64 global-instance gate idle runtime success

Date: 2026-09-04 KST

## Scope

Hardware validation of the first native ARM64 ASIO coexistence gate derived from static analysis of Creative `CtU2As64.DLL`.

Experiment branch:

- `exp/windows-arm64-asio-global-instance-gate`
- tested source HEAD: `362d58372b58640ac666dd59f17e532b092c05d3`
- baseline ancestor: `a02be3c7ffb4dc66c7eb903712a8b4301efe8ea7`

The experiment changes only the `KsCreatePin` entry path by querying Render Pin 1 `KSPROPERTY_PIN_GLOBALCINSTANCES` first. No other WaveRT format, buffer, state, notification, or teardown behavior is intentionally changed.

## Hardware condition

Sound Blaster X4 connected normally.

No competing Windows playback stream active on X4 during this run.

## Gate result

Observed before the real `KsCreatePin` call:

```text
GLOBAL INSTANCE GATE: PinId=1 PossibleCount=1 CurrentCount=0 busy=NO
GLOBAL INSTANCE GATE: FREE -> calling real KsCreatePin
```

Therefore, with X4 idle:

- `PossibleCount = 1`
- `CurrentCount = 0`
- gate result = FREE
- real `KsCreatePin` is allowed

## Existing baseline lifecycle after the gate

The unchanged SDK baseline path then completed successfully:

- WaveRT buffer returned
  - `ActualBufferSize = 4096`
  - `CallMemoryBarrier = 0`
- `KSSTATE_ACQUIRE` OK
- `KSSTATE_PAUSE` OK
- `KSSTATE_RUN` OK
- 20/20 notifications observed
- packet count advanced 1 through 20
- no packet discontinuities
- no presentation-position regressions
- RUN -> PAUSE -> ACQUIRE -> STOP succeeded
- notification unregister succeeded
- clean close succeeded

Final result:

```text
notifications=20
packet_discontinuities=0
position_regressions=0
SDK ABI BASELINE RESULT: PASS
```

## Conclusion

The global-instance gate itself does not disturb the known-good single-stream WaveRT lifecycle when the X4 render pin has available global capacity.

This validates the FREE half of the intended coexistence gate on hardware.

## Next single step

Run the exact same fixed executable once while normal Windows playback through X4 is actively running.

Expected safe behavior based on the prior read-only preflight result:

```text
GLOBAL INSTANCE GATE: PinId=1 PossibleCount=1 CurrentCount=1 busy=YES
GLOBAL INSTANCE GATE: BUSY -> KsCreatePin SKIPPED
```

The active-playback validation is successful only if `KsCreatePin` is not reached and no kernel crash occurs.

Do not change format, WaveRT buffer geometry, notification behavior, sample rate, channel count, or lifecycle in this test.

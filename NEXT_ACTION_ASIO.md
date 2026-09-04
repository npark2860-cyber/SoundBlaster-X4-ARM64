# NEXT ACTION — Native ARM64 / ARM64EC ASIO

Updated: 2026-09-04 KST

## Current milestone

The native B5 control panel first-pass implementation is complete enough to freeze unless a concrete issue appears.

Read first:

1. `CURRENT_HANDOFF.md`
2. `NEXT_ACTION_ASIO_CONTROL_PANEL.md`
3. `DEBUG_HISTORY_20260904_ASIO_B5_POST_COALESCE_STALE_WAKE.md`
4. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V4_STATS_ALIAS_REGRESSION.md`

GitHub is source of truth. Verify current refs before editing.

---

## Runtime base to preserve

Current B5 productization branch:

`exp/windows-arm64-asio-b5-capability-productization@4475fc17b70f372fe317fa201f201e8dc5543f9f`

Current control-panel branch:

`exp/windows-arm64-asio-b5-control-panel@7bc83f87b172f574064d086affe5e8ed1d6fbdff`

Control-panel work preserves the validated runtime marker set:

- `dual-event-mux-v4-coalesce-recovery`
- `runtime-failsafe-v1`
- `packet-stats-observed-v1`
- `post-coalesce-stale-v1`

Do not weaken runtime safety merely to simplify host/UI behavior.

Validated B4D fallback remains frozen.

---

## Control-panel status

ARM64EC / REAPER user evidence:

- panel opens;
- REAPER sample rate was changed/applied to 96,000 Hz;
- after ASIO reopen/reinitialization the panel still showed `96,000 Hz`;
- current user report: no apparent panel problem.

The forced 48 kHz reset in `init()` was removed and successful supported `setSampleRate()` selections now persist per user.

Latest panel build also replaces the mojibake-prone Unicode bullet with ASCII `|`.

ARM64EC and Classic ARM64 compile/link both pass. Classic runtime panel invocation is not yet separately verified.

Do not spend another cycle refactoring the panel unless a concrete issue is observed.

---

## Immediate ASIO engineering priority

Return to the separate unresolved B5 runtime closure item:

**192 kHz / 384-frame long-cadence strict forward `delta=3`.**

Current known cadence state on the validated productization runtime:

- 192k/384: strict FAIL on a forward `delta=3`
- 192k/432: PASS x2 with real +2 coalesce / stale-wake recovery exercised
- 192k/480: PASS x2 with recovery exercised
- 192k/576: PASS x2 with recovery exercised

`post-coalesce-stale-v1` is proven for the measured `+2 -> same packet once` sequence. The 384-frame `delta=3` is a different condition and remains fatal by design.

Do not simply relax `delta > 2` to make the probe pass.

---

## Next runtime investigation rules

The next runtime work should isolate why 192k/384 can advance by three packets under long cadence while larger accepted geometries remain recoverable.

Preserve these constraints:

- do not rerun allocation geometry without contradictory evidence;
- 384 remains the first hardware-accepted allocation size;
- do not change the public minimum merely to hide the cadence problem;
- keep B4D unchanged;
- keep BUSY and pre-pin ownership gates unchanged;
- keep worker-join and fatal-silence behavior unchanged;
- do not mix control-panel UI changes into the runtime experiment.

Prefer one-variable runtime instrumentation/analysis over broad rewrites.

---

## After 192k/384 closure

Only after the separate runtime item is understood and safely handled:

1. confirm the final B5 runtime matrix;
2. finish any still-required real output / real stereo input validation at 48/96 kHz;
3. decide whether to merge/freeze the control-panel branch into the release line;
4. freeze the B5 first release;
5. resume deferred CTCDC/CTIntrfu work if the user chooses that track.

Do not ask the user to repeat already completed audible REAPER or geometry tests without a concrete reason.

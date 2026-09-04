# NEXT ACTION — Native ARM64 / ARM64EC ASIO

Updated: 2026-09-04 KST

## Immediate milestone

Proceed with the native B5 ASIO control panel in a fresh work tab.

Read first:

1. `CURRENT_HANDOFF.md`
2. `NEXT_ACTION_ASIO_CONTROL_PANEL.md`
3. `PROMPT_ASIO_CONTROL_PANEL.md`
4. `DEBUG_HISTORY_20260904_ASIO_B5_POST_COALESCE_STALE_WAKE.md`

GitHub is source of truth. Verify actual refs before editing.

---

## Runtime state to preserve

Validated B4D fallback remains frozen:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Earlier real-audible REAPER reference:

`ca37f0e8427227733cd6082a50e20101312e3333`

The user explicitly reported ordinary REAPER playback was normal on that built bundle.

Current B5 productization branch:

`exp/windows-arm64-asio-b5-capability-productization@4475fc17b70f372fe317fa201f201e8dc5543f9f`

Latest runtime-code commit:

`64e34b48714789ab17fba57be34b054f2170b4e9`

This current branch is no longer unbuilt. The latest `post-coalesce-stale-v1` bundle was built and returned a full product validation PASS on 2026-09-04 16:32:53.73.

Latest product matrix PASS includes:

- 48k/240 output x3
- 48k/240 duplex x2
- 96k/240 duplex x2
- REAPER-matched 48k/480 output for 5 seconds: 500 callbacks, stop=0, workerJoined=YES
- 192k/384 output x2
- 48k/96
- 48k/4800
- 48k/512 compatibility
- ASIO capability probe

Latest dedicated 192 kHz cadence result:

- 384 frames: strict FAIL on `1375 -> 1378`, delta=3
- 432 frames: PASS x2 while actually recovering +2 coalesces and consuming post-coalesce stale wakes
- 480 frames: PASS x2 with the same recovery path exercised
- 576 frames: PASS x2 with recovery exercised

Therefore `post-coalesce-stale-v1` is proven to work for the measured `+2 -> same packet once` sequence, but B5 runtime is not completely closed because 192k/384 can still produce an unrecoverable forward delta=3 in the long cadence probe.

Do not weaken `delta > 2` from the control-panel work. Keep that runtime issue separate.

The 192 kHz allocation geometry is unchanged: 384 is still the first accepted size and 432..960 are accepted. Do not rerun geometry without contradictory evidence.

---

## Control-panel target

Current behavior:

`ASIOError controlPanel() override { return ASE_NotPresent; }`

Replace this with an own native Win32 panel. No Creative binary reuse.

Required first-release behavior:

- compact credible production UI
- `Sound Blaster X4 ARM64 ASIO B5` identity
- current/effective sample rate
- current/effective buffer
- frames + milliseconds
- 48/96 kHz: 96..4800, step 48, preferred/default 240
- 192 kHz: 384..4800, step 48, preferred/default 384
- 512 compatibility option
- show an active host buffer such as 480 as current/effective, not merely the fixed preferred 240
- opening panel creates no WaveRT pins and does not alter hardware state
- no live geometry mutation while buffers/RUN are active
- deterministic Apply / OK / Cancel
- persist the user's next-safe-open preference
- explicitly define whether host reset/reopen is required
- retain a lightweight diagnostics surface without callback-path file I/O

Important: the ASIO host supplies `bufferSize` to `createBuffers()`. Do not silently replace an explicit host buffer request with the panel preference. If `kAsioResetRequest` is considered, inspect callback lifetime/reentrancy first.

---

## Isolation

Preferred control-panel work branch:

`exp/windows-arm64-asio-b5-control-panel`

Preferred base after fresh ref verification:

`4475fc17b70f372fe317fa201f201e8dc5543f9f`

Reason: this current B5 source has now been built and product-matrix validated and contains the verified one-shot post-coalesce stale-wake behavior.

However, freeze the WaveRT/mux runtime files on the UI branch. The known 192k/384 long-cadence delta=3 issue is a separate runtime task and must not be mixed into control-panel implementation.

Do not alter B4D. Do not refactor WaveRT/mux code for UI cleanliness.

---

## Validation

Panel-first validation:

1. ARM64EC + Classic ARM64 build PASS;
2. correct side-by-side registration;
3. host can invoke the panel;
4. open/cancel/close creates no WaveRT pins;
5. current sample rate/buffer display is correct;
6. buffer choices obey rate-specific contract + 512 exception;
7. Cancel is no-op;
8. Apply/OK persistence is deterministic;
9. active buffers/RUN cannot be mutated unsafely;
10. any reset/reopen notification is separately validated.

After panel PASS, run the normal B5 product matrix and one ordinary REAPER check. Then close the separate 192k/384 delta=3 runtime item, finish real output + real stereo input validation at 48/96 kHz, freeze B5 first release, and resume CTCDC/CTIntrfu work.

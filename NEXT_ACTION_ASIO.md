# NEXT ACTION — Native ARM64 / ARM64EC ASIO

Updated: 2026-09-04 KST

## Immediate milestone

Proceed with the native B5 ASIO control panel in a fresh work tab.

Read first:

1. `CURRENT_HANDOFF.md`
2. `NEXT_ACTION_ASIO_CONTROL_PANEL.md`
3. `PROMPT_ASIO_CONTROL_PANEL.md`

GitHub is source of truth. Verify actual refs before editing.

---

## Runtime state to preserve

Validated B4D fallback remains frozen:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Last runtime-validated B5 reference:

`ca37f0e8427227733cd6082a50e20101312e3333`

That built bundle returned full product-validation PASS and the user subsequently reported normal ordinary REAPER playback at the already observed 48 kHz / 480-sample host setting.

Current B5 productization branch is newer:

`exp/windows-arm64-asio-b5-capability-productization@4475fc17b70f372fe317fa201f201e8dc5543f9f`

Latest runtime code there:

`64e34b48714789ab17fba57be34b054f2170b4e9`

It adds `post-coalesce-stale-v1`, but **that newer runtime delta is still unbuilt/unvalidated**. Keep it separate from the validated reference and do not make panel work depend on it.

---

## Control-panel target

Current validated-source behavior:

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

Use validated `ca37f0e...` as the runtime reference/base unless fresh GitHub inspection gives a concrete reason not to. Do not mix the unvalidated stale-wake runtime delta into UI work implicitly. Reconcile the two explicitly later.

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

After panel PASS, run the normal B5 product matrix and one ordinary REAPER check. Then finish real output + real stereo input validation at 48/96 kHz, freeze B5 first release, and resume CTCDC/CTIntrfu work.

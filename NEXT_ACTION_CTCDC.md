# NEXT ACTION — CTCDC Unlock / Session Reconstruction

Updated: 2026-09-03 KST

## Objective

Recover the exact native Windows CTCDC session required before X4 accepts passthrough MIDAS commands, then reproduce Direct Mode ON/OFF on Windows ARM64 without relying on Creative's x64-only stack.

Known Direct Mode frame is already confirmed:

- OFF `5A 39 03 00 05 00`
- ON  `5A 39 03 00 05 01`

Do not spend time rediscovering these bytes.

## Required startup files

Primary native inputs supplied by user:

### CTCDC.dll

- size `2,122,200`
- SHA-256 `bc4010e8f7000bfe6217425a0622dd710a7626d90fb61008505337aa87a43dab`

### CTIntrfu.dll

- size `109,656`
- SHA-256 `ecf098101a0663568f4a406d7bed9775565a67213930e2487c17d858a5d0d9b6`

These binaries are not in GitHub. If the new chat cannot access them, ask the user to upload these exact two files again.

Useful managed reference binaries if needed:

- `Creative.Platform.Devices.dll`
  - SHA-256 `2d77172fb6ae850b6d03a09830892c8c3a0ab79e10dda28f40a76b3fadc47e93`
- `Creative.App.UI.Framework.dll`
  - SHA-256 `f903fc410528a314fc890df53766d19dad11efb0b5074017f3e71de4d905f8d8`

## Existing experimental branch

`poc/windows-arm64-usb-serial-ctcdc-init`

HEAD at handoff:

`d44a33936639cc76c935b59c0502133eaa5bcf2d`

Successful ARM64 build run:

`33692026928`

Final workflow state is manual-only (`workflow_dispatch`).

Do not merge this branch into `main` until the hardware behavior is understood.

## Execution order

### Stage A — Native static analysis first

Trace `CTIntrfu.dll`:

1. `CTCreateInstanceEx`
2. module/class resolution for CTCDC
3. construction of `ICTCDC`
4. exact lifetime / initialize / open / close sequence
5. arguments passed into CTCDC and any hidden configuration values

Trace `CTCDC.dll`:

1. `ICTCDC::Initialize`
2. `ICTCDC::Open`
3. serial-port discovery/open logic
4. DCB/event-mask/timeouts/purge/DTR configuration
5. first `5A 03 00` query
6. command-`0x03` response parser
7. condition that enters unlock mode
8. literal `whoareyou.MyApp8\r\n` path
9. challenge parser
10. cryptographic/key transform for the reply
11. exact bytes/string written as unlock response
12. success/failure parser
13. SetSoftwareMode path
14. any additional post-unlock initialization
15. `ExecuteCommand(1001)` implementation
16. how raw passthrough responses are framed/read

Record function offsets/RVAs and pseudocode in a new debug history document. Do not only record strings.

### Stage B — Verify the existing CTCDC probe against the binary

The branch currently reproduces:

- event mask `0x05`
- 115200 baud
- 8N1
- zero timeouts
- `PurgeComm(0x0F)`
- `SETDTR`
- first query `5A 03 00`
- unlock greeting `whoareyou.MyApp8\r\n`

Compare each of these against the exact supplied `CTCDC.dll` before extending the probe.

If static analysis contradicts the branch, fix the branch based on binary evidence rather than preserving the old probe behavior.

### Stage C — Controlled hardware observation

Only after Stage A/B is coherent:

Run the existing safe probe and capture:

`x4-ctcdc-probe.txt`

Required observations:

- whether `5A 03 00` receives a valid response
- exact response bytes
- whether unlock greeting is reached
- exact challenge/response text or bytes returned by X4

Do not send a guessed unlock reply.

### Stage D — Implement exact unlock/session response

After the algorithm is recovered statically and the device challenge is known:

- implement only the exact Creative-observed response algorithm
- add verbose hex logging for every TX/RX
- stop immediately on unexpected response
- do not auto-loop destructive or unknown commands

Validate unlock/software mode first without Direct Mode.

### Stage E — Direct Mode passthrough

When CTCDC session setup is confirmed:

Send exactly one known command at a time:

ON:

`5A 39 03 00 05 01`

OFF:

`5A 39 03 00 05 00`

Require physical X4 state confirmation from the user.

A successful OS-level write alone is not validation.

## Important known native/API clues

Managed layer calls:

`ICTCDC.ExecuteCommand(1001, ...)`

where 1001 is `CTCDCCMD_WritePassthroughData`.

Observed/previously identified COM-style IDs:

- CCTCDC CLSID `{66FC4CF0-56C8-4523-A92B-CE69FCD7556A}`
- ICTCDC IID `{669E9C0E-AD66-48C3-8228-29A55C2E9977}`

CTCDC error enumeration included indications such as:

- Failed Unlock
- Failed SetSoftwareMode

Treat these as roadmap clues, not a substitute for tracing the actual control flow.

## Do not regress to eliminated paths

Do not restart:

- Windows BLE implementation
- HID output experiments
- raw Direct Mode COM write without session setup
- UAC Extension Unit search
- vendor-interface search
- `6A` / guessed `5C` Direct Mode frame variants

## Documentation discipline

For every new confirmed step, write a dated GitHub document before moving on.

Suggested next document name:

`DEBUG_HISTORY_20260903_CTCDC_NATIVE_UNLOCK_TRACE.md`

That document should distinguish:

- binary-confirmed fact
- inference
- hardware-confirmed runtime result

Never promote an inference to confirmed state until there is static or runtime evidence.

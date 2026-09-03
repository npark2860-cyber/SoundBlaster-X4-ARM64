# NEXT ACTION — CTCDC Hardware Challenge / Session Validation

Updated: 2026-09-03 KST

## Current status

Native static analysis **Stage A is complete** for the exact supplied binaries, and the existing safe ARM64 probe audit/fix **Stage B is complete**.

Do not repeat the native trace unless the input DLL hashes change.

Full binary trace:

`DEBUG_HISTORY_20260903_CTCDC_NATIVE_UNLOCK_TRACE.md`

Current next step is **Stage C — controlled hardware observation**.

## Fixed binary identities

### CTCDC.dll

- size `2,122,200`
- SHA-256 `bc4010e8f7000bfe6217425a0622dd710a7626d90fb61008505337aa87a43dab`

### CTIntrfu.dll

- size `109,656`
- SHA-256 `ecf098101a0663568f4a406d7bed9775565a67213930e2487c17d858a5d0d9b6`

Both supplied binaries are PE32/x86. They are reference inputs for reconstructing an independent ARM64 implementation; they are not ARM64-loadable components.

## Binary-confirmed session state machine

Relevant path is now recovered as:

`CTCreateInstanceEx`
→ `DllGetClassObject`
→ `IClassFactory::CreateInstance`
→ `ICTCDC::Initialize`
→ `ICTCDC::Open`
→ serial init
→ `5A 03 00` GetMaximumPayloadSize
→ if unavailable, unlock greeting
→ challenge parse
→ AES-256-GCM unlock reply
→ `unlock_OK\r\n`
→ `SW_MODE1\r\n`
→ `5A 03 00` GetMaximumPayloadSize retry
→ `5A 09 01 02` GetFirmwareVersionString
→ `5A 26 01 05` QueryButtonsAvailable
→ normal session
→ `ICTCDC.ExecuteCommand(1001, ...)`
→ raw MIDAS bytes

Known Direct Mode raw frames remain:

- OFF `5A 39 03 00 05 00`
- ON `5A 39 03 00 05 01`

`ExecuteCommand(1001)` adds no extra MIDAS wrapper.

## Important correction

`5A 03 00` is **not** a firmware-version query.

It is:

`CTCDCCMD_GetMaximumPayloadSize`

A valid command-`0x03`, 2-byte response yields the maximum payload size and is used by CTCDC as a session-readiness test.

## Recovered unlock algorithm

Normal challenge layout:

- bytes `0..8`: ASCII `whoareyou`
- bytes `9..12`: seed4
- bytes `13..44`: challenge32

Effective AES-256 key:

`seed[0:2] || D3 1A 21 27 9B E3 46 F0 99 9D 6E C4 C3 FE BE 98 90 18 69 C1 18 FB B1 25 6E 0C E0 7B || seed[2:4]`

Normal unlock reply is exactly 72 bytes:

`"unlock" || random16 || ciphertext32 || tag16 || "\r\n"`

- cipher: AES-256-GCM
- first 12 bytes of `random16`: GCM nonce
- plaintext input: challenge32
- AAD: none observed
- tag: 16 bytes
- expected success response: `unlock_OK\r\n`

Do not implement a different or guessed challenge transform.

## Existing safe probe

Branch:

`poc/windows-arm64-usb-serial-ctcdc-init`

Current HEAD after Stage B corrections:

`2125308b869fae21cef3d074de1e7a7a0e250b27`

Workflow:

`Build X4 CTCDC Serial Probe ARM64`

Trigger remains manual-only:

`workflow_dispatch`

The Stage B correction changed only probe source/documentation:

- `5A 03 00` is now parsed/logged as maximum payload size
- unrelated DCB flags are preserved instead of forcibly cleared
- `GetCommTimeouts` is read before zeroing the timeout structure
- no unlock reply, `SW_MODE1`, or Direct Mode command was added

The workflow itself was not changed.

## Stage C — run now

Manually run the branch workflow and execute the resulting ARM64 probe against the locally USB-connected X4.

Capture:

`x4-ctcdc-probe.txt`

Required observations:

1. exact RX bytes after `5A 03 00`
2. whether a valid maximum-payload response is already returned
3. if not, whether `whoareyou.MyApp8\r\n` is sent
4. exact RX bytes from the unlock stage
5. if the reply begins with `whoareyou`, preserve the full seed/challenge bytes exactly

The current probe intentionally stops before generating/sending the cryptographic unlock reply.

## Stage D — only after Stage C capture

Using the hardware challenge plus the already recovered binary algorithm:

1. implement the exact 72-byte AES-256-GCM unlock response
2. log every TX/RX in hex
3. require exact `unlock_OK\r\n`
4. send exact `SW_MODE1\r\n`
5. validate its expected success response
6. re-run `5A 03 00`
7. validate `5A 09 01 02`
8. stop before Direct Mode

Do not send guessed replies or unknown commands.

## Stage E — Direct Mode passthrough

Only after Stage D proves the session state:

- ON: `5A 39 03 00 05 01`
- OFF: `5A 39 03 00 05 00`

Send one command at a time and require physical X4 state confirmation.

A successful Windows write is not sufficient validation.

## Do not regress

Do not restart:

- Windows BLE control
- HID output experiments
- naked Direct Mode COM writes without session setup
- UAC Extension Unit search
- vendor-class interface search
- `6A` Direct Mode variants
- guessed `5C` wrappers

## Documentation discipline

For every new hardware-confirmed step, add a dated GitHub debug-history document before extending the probe further.

Keep these categories distinct:

- binary-confirmed fact
- inference
- hardware-confirmed runtime result

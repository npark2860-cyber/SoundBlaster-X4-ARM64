# NEXT ACTION — CTCDC Open-Session Validation

Updated: 2026-09-03 KST

## Current status

Native static analysis is complete for the exact supplied `CTCDC.dll` / `CTIntrfu.dll` hashes.

Hardware Stage C1 is also complete.

The physical X4 replied to:

`5A 03 00`

with:

`5A 03 02 3B 00`

Binary-confirmed interpretation:

- command `0x03` = `CTCDCCMD_GetMaximumPayloadSize`
- payload `3B 00` = `0x003B` = **59 bytes**

This is the successful first branch of `ICTCDC::Open()`.

Therefore, in the currently observed X4 runtime state:

- **do not enter Unlock**
- **do not send `SW_MODE1`**
- continue directly to the remaining CTCDC `Open()` queries

Runtime evidence document:

`DEBUG_HISTORY_20260903_CTCDC_MAX_PAYLOAD_RUNTIME.md`

Full native trace:

`DEBUG_HISTORY_20260903_CTCDC_NATIVE_UNLOCK_TRACE.md`

---

## Exact binary identities

### CTCDC.dll

- size `2,122,200`
- SHA-256 `bc4010e8f7000bfe6217425a0622dd710a7626d90fb61008505337aa87a43dab`
- PE32/x86

### CTIntrfu.dll

- size `109,656`
- SHA-256 `ecf098101a0663568f4a406d7bed9775565a67213930e2487c17d858a5d0d9b6`
- PE32/x86

These are reference binaries for reconstructing an independent ARM64 implementation.

---

## Current hardware-confirmed path

`COM3 open/configure`
→ `5A 03 00` GetMaximumPayloadSize
→ RX `5A 03 02 3B 00`
→ Maximum Payload Size = 59
→ skip Unlock
→ skip `SW_MODE1`
→ **next: `5A 09 01 02` GetFirmwareVersionString**
→ **then: `5A 26 01 05` QueryButtonsAvailable**
→ normal CTCDC session
→ later `ExecuteCommand(1001)` raw passthrough

The known Direct Mode frames remain:

- OFF `5A 39 03 00 05 00`
- ON `5A 39 03 00 05 01`

Do not send them yet.

---

## Stage C2 — run now

Branch:

`poc/windows-arm64-usb-serial-ctcdc-init`

Current branch HEAD after the Stage C2 probe update:

`1b95f0734e2a2a26d9bc606809b9e04942d1808a`

Workflow:

`Build X4 CTCDC Serial Probe ARM64`

Trigger remains manual-only:

`workflow_dispatch`

The current build target is `session-open-probe.cpp`.

It performs only:

1. CTCDC serial initialization
2. `5A 03 00` — require a valid Maximum Payload Size response
3. `5A 09 01 02` — capture/validate `GetFirmwareVersionString`
4. `5A 26 01 05` — capture/validate `QueryButtonsAvailable`
5. stop

It does **not** send:

- unlock response
- `SW_MODE1`
- Direct Mode
- unknown/state-changing commands

Run the manually built ARM64 probe and upload:

`x4-ctcdc-probe.txt`

Required evidence:

- exact RX bytes for `5A 09 01 02`
- exact RX bytes for `5A 26 01 05`
- whether both frames match the native CTCDC parser expectations

---

## After Stage C2 succeeds

Do not implement the AES unlock path unless a future runtime state actually fails `GetMaximumPayloadSize`.

For the currently responsive state, the next controlled step is a dedicated passthrough test that reproduces the already-confirmed `ExecuteCommand(1001)` behavior and sends exactly one Direct Mode frame at a time.

First Direct Mode validation should still require physical X4 state confirmation; successful `WriteFile` alone is not proof.

---

## Unlock path — contingency only

The unlock algorithm remains fully recovered for a future state where `5A 03 00` fails.

Normal challenge:

`whoareyou || seed4 || challenge32`

Effective AES-256 key:

`seed[0:2] || D3 1A 21 27 9B E3 46 F0 99 9D 6E C4 C3 FE BE 98 90 18 69 C1 18 FB B1 25 6E 0C E0 7B || seed[2:4]`

Reply:

`"unlock" || random16 || AES-256-GCM(ciphertext32) || tag16 || "\r\n"`

Expected success:

`unlock_OK\r\n`

Then:

`SW_MODE1\r\n`
→ `5A 03 00`
→ `5A 09 01 02`
→ `5A 26 01 05`

Do not force this path when the initial maximum-payload query already succeeds.

---

## Do not regress

Do not restart:

- Windows BLE control
- HID output experiments
- naked Direct Mode COM writes without CTCDC session setup
- UAC Extension Unit search
- vendor-class interface search
- `6A` Direct Mode variants
- guessed `5C` wrappers

## Documentation discipline

For each new hardware-confirmed step, add a dated debug-history document before extending the probe.

Keep these categories distinct:

- binary-confirmed fact
- hardware-confirmed runtime result
- inference

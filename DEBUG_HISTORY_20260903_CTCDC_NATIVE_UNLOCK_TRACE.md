# DEBUG HISTORY — CTCDC Native Unlock / Session Trace (2026-09-03)

## Scope

This document records static analysis of the exact user-supplied native binaries used by the Creative Windows control path.

Binary identities:

- `CTCDC.dll`
  - size: `2,122,200` bytes
  - SHA-256: `bc4010e8f7000bfe6217425a0622dd710a7626d90fb61008505337aa87a43dab`
  - PE32 / x86 (`IMAGE_FILE_MACHINE_I386`)
  - image base: `0x10000000`
- `CTIntrfu.dll`
  - size: `109,656` bytes
  - SHA-256: `ecf098101a0663568f4a406d7bed9775565a67213930e2487c17d858a5d0d9b6`
  - PE32 / x86 (`IMAGE_FILE_MACHINE_I386`)
  - image base: `0x10000000`

The binaries themselves are not committed to this repository.

Unless explicitly marked otherwise, findings below are **binary-confirmed static-analysis facts**. No new hardware runtime result was available while writing this document.

---

## 1. CTIntrfu factory path

`CTIntrfu.dll` exports:

- `CTCreateInstanceEx` at VA `0x10004610` / RVA `0x4610`

`CTCreateInstanceEx` forwards into the generic Creative component creation path. The resolver uses Creative component-installation registry locations including:

- `SOFTWARE\Creative Tech\Component Installed\Category\`
- `SOFTWARE\Creative Tech\Component Installed\`

The loader path then performs:

1. resolve the component DLL/module path
2. `LoadLibraryExW(..., flags=8)`
3. `GetProcAddress(module, "DllGetClassObject")`
4. call `DllGetClassObject` with the requested component CLSID and standard `IID_IClassFactory`
5. call class-factory vtable slot `+0x0C` (`CreateInstance`) with the requested interface IID

The `IID_IClassFactory` constant is the standard:

`{00000001-0000-0000-C000-000000000046}`

`CTIntrfu.dll` is a generic Creative component loader; the CTCDC-specific CLSID/IID are not hardcoded as its generic factory identity.

---

## 2. CTCDC COM identity and ICTCDC vtable

Exact identifiers present in `CTCDC.dll`:

- CCTCDC CLSID: `{66FC4CF0-56C8-4523-A92B-CE69FCD7556A}`
- ICTCDC IID: `{669E9C0E-AD66-48C3-8228-29A55C2E9977}`

The ICTCDC vtable resolves to:

| vtable slot | VA | role |
|---|---:|---|
| `+0x00` | `0x10001E60` | `QueryInterface` |
| `+0x04` | `0x10001E80` | `AddRef` |
| `+0x08` | `0x10001EA0` | `Release` |
| `+0x0C` | `0x10003430` | `Initialize` |
| `+0x10` | `0x100035F0` | `Open` |
| `+0x18` | `0x10003BE0` | `ExecuteCommand` |
| `+0x1C` | `0x10003DB0` | unregister notification callback path |
| `+0x20` | `0x10003DF0` | `Close` |
| `+0x24` | `0x10003EE0` | `Shutdown` |
| `+0x28` | `0x10003FD0` | device enumeration path |

`QueryInterface` compares against the exact ICTCDC IID above.

---

## 3. Initialize configuration

`ICTCDC::Initialize` is at VA `0x10003430`.

Observed configuration source/keys:

- file: `CTCDC.dat`
- section: `Debug`
- `DebugMode`
- `TimeOutToWaitForCdcResponseInMilliseconds`
- `MaxNumRetriesWhenNoCdcResponseForReadAndWrite`
- `DoNotAutoUnlock`
- `DoNotReleaseComPortUponRequest`

`DoNotAutoUnlock` is stored at object offset `+0x84`. Its normal default is zero, so automatic session unlock behavior is enabled unless configured otherwise.

---

## 4. Serial initialization — exact behavior

The serial open/configuration helper is at VA `0x10004D50`.

Confirmed sequence:

1. `GetCommMask`
2. `SetCommMask(handle, 0x05)` = `EV_RXCHAR | EV_TXEMPTY`
3. `GetCommState`
4. modify DCB:
   - baud = `115200`
   - byte size = `8`
   - parity = none
   - stop bits = one
   - DCB flag word is masked with `0xFFFFCFCF`
5. `SetCommState`
6. `GetCommTimeouts`
7. zero all five `COMMTIMEOUTS` DWORDs
8. `SetCommTimeouts`
9. `PurgeComm(handle, 0x0F)`
10. `EscapeCommFunction(handle, SETDTR)`

The DCB mask `0xFFFFCFCF` clears the DTR-control and RTS-control mode bits while preserving unrelated pre-existing flow-control flags.

### Probe correction required

The current ARM64 probe explicitly clears several unrelated DCB flags. That is not an exact reproduction of CTCDC and must be corrected to preserve those flags.

---

## 5. `5A 03 00` meaning corrected

The previous probe labeled:

`5A 03 00`

as a firmware query.

That interpretation is incorrect.

The exact CTCDC function is:

- VA `0x10008600`
- `DoExecuteCommand_CTCDCCMD_GetMaximumPayloadSize`

TX:

`5A 03 00`

Response parser accepts both `5A` and `5B` framing and requires:

- command `0x03`
- payload size `2`

The returned 16-bit word is **Maximum Payload Size**, not firmware version.

This query is also used as the session-readiness test inside `Open()`.

---

## 6. `Open()` session state machine

`ICTCDC::Open` is at VA `0x100035F0`.

When `DoNotAutoUnlock == 0`, the binary-confirmed control flow is:

1. open/configure the serial port
2. call `GetMaximumPayloadSize` (`5A 03 00`)
3. if that succeeds:
   - save the maximum payload size
   - skip unlock and `SetSwMode1`
4. if it fails:
   - clear stored payload size
   - call Unlock
   - retry Unlock at most once after a deliberate `Sleep(1000)` delay, for up to two Unlock attempts total
   - call `SetSwMode1`
   - call `GetMaximumPayloadSize` again
   - require the retry to succeed before continuing
5. perform `GetFirmwareVersionString`
6. perform `QueryButtonsAvailable`
7. continue normal internal state/callback setup

Implementation quirk: immediately after the `SetSwMode1` call, one branch tests the register still holding the preceding Unlock result rather than first replacing it with the `SetSwMode1` return value. The following `GetMaximumPayloadSize` retry is nevertheless executed and its own result gates further progress. This is recorded as an implementation detail only.

---

## 7. Unlock greeting and challenge handling

Unlock function:

- VA `0x10008380`

Initial TX is exactly 18 bytes:

`whoareyou.MyApp8\r\n`

Read timeout used by this stage: `3000 ms`.

Special device replies handled by the response builder:

- `Unknown command\r\n`
  - treated as old firmware for which no unlock reply is necessary
- `NotYet\r\n`
  - delayed/rechecked path with one-second sleeps and a bounded retry count
- normal challenge begins with exact prefix `whoareyou`

For the normal challenge, the binary directly consumes:

- bytes `0..8`: `whoareyou`
- bytes `9..12`: 4-byte seed
- bytes `13..44`: 32-byte challenge payload

---

## 8. Unlock key construction

The 32-byte key template in `CTCDC.dll` is:

`4F 41 D3 1A 21 27 9B E3 46 F0 99 9D 6E C4 C3 FE BE 98 90 18 69 C1 18 FB B1 25 6E 0C E0 7B 6F 0A`

Before use:

- key bytes `0..1` are overwritten with seed bytes `0..1`
- key bytes `30..31` are overwritten with seed bytes `2..3`

Therefore the effective AES-256 key is:

`seed[0:2] || fixed[28] || seed[2:4]`

where the fixed middle 28 bytes are:

`D3 1A 21 27 9B E3 46 F0 99 9D 6E C4 C3 FE BE 98 90 18 69 C1 18 FB B1 25 6E 0C E0 7B`

---

## 9. Unlock cryptography — AES-256-GCM

This is binary-confirmed, not inferred from strings alone.

The internal cipher table includes an `AES-256-GCM` descriptor with:

- key size: 256 bits
- IV/nonce size: 12 bytes
- authentication enabled
- block size: 16 bytes

The unlock builder generates 16 pseudo-random bytes. The GCM initializer consumes the **first 12 bytes** as the nonce and constructs the standard 96-bit-nonce counter start `IV || 00000001`.

The 32-byte challenge payload is encrypted with AES-256-GCM. No AAD operation is used in this path.

The final GCM authentication tag is 16 bytes.

### Random generator used by this binary

The path seeds the MSVC-style `rand()` state from a time-like value and then generates 16 bytes from the low byte of successive `rand()` outputs.

The internal LCG is the standard form observed in the binary:

`state = state * 0x343FD + 0x269EC3`

`rand = (state >> 16) & 0x7FFF`

All 16 generated bytes are transmitted. Only the first 12 are consumed as the GCM nonce; no additional cryptographic role for the final four bytes was observed in this path.

---

## 10. Exact unlock response layout

The normal generated response is exactly 72 bytes:

1. ASCII `unlock` — 6 bytes
2. generated random field — 16 bytes
3. AES-256-GCM ciphertext of the 32-byte challenge — 32 bytes
4. GCM authentication tag — 16 bytes
5. CRLF — 2 bytes

Layout:

`"unlock" || random16 || ciphertext32 || tag16 || "\r\n"`

The device success reply is compared against exactly 11 bytes:

`unlock_OK\r\n`

---

## 11. Set software mode

`SetSwMode1` function:

- VA `0x10008510`

TX is exactly the ASCII command:

`SW_MODE1\r\n`

Length: 10 bytes.

Read timeout: `3000 ms`.

The response parser expects the CTCDC command-`0x02` response path and selector value `0x6D`, with a zero success status.

This stage occurs after successful Unlock and before the second `GetMaximumPayloadSize` readiness query.

---

## 12. Required post-session firmware query

After the session/readiness sequence succeeds, `Open()` dispatches command `12` (`0x0C`):

`CTCDCCMD_GetFirmwareVersionString`

Implementation VA:

- `0x1000B060`

TX:

`5A 09 01 02`

Read timeout: `3000 ms`.

The parser matches command `0x09`, selector `0x02`, and extracts the firmware-version string.

Unlike `QueryButtonsAvailable` below, failure of this command is on the `Open()` failure path. Therefore it is part of the required successful `Open()` sequence.

---

## 13. Post-session button capability query

`Open()` then calls:

`DoExecuteCommand_CTCDCCMD_QueryButtonsAvailable`

Implementation VA:

- `0x1000AA60`

TX:

`5A 26 01 05`

Read timeout: `3000 ms`.

The observed caller does not gate overall `Open()` success on this query result. It is a capability-population step rather than the primary session gate.

---

## 14. ExecuteCommand and passthrough

Public `ICTCDC::ExecuteCommand`:

- VA `0x10003BE0`

Internal dispatcher:

- VA `0x10005610`

### Command 1000 / `0x3E8` — ReadPassthroughData

Parameter structure is used as:

- `+0x00`: destination buffer pointer
- `+0x04`: requested size
- `+0x08`: output bytes read

It consumes raw data from CTCDC's receive path.

### Command 1001 / `0x3E9` — WritePassthroughData

Parameter structure is used as:

- `+0x00`: source buffer pointer
- `+0x04`: byte size

The implementation ultimately passes these bytes to the raw serial write path (`WriteFile`).

**No additional MIDAS wrapper is added by command 1001.**

Therefore, once the CTCDC session is correctly established, the managed Direct Mode frame remains exactly:

- OFF: `5A 39 03 00 05 00`
- ON: `5A 39 03 00 05 01`

The missing piece in the failed naked-COM experiment was the CTCDC session state, not another guessed wrapper around these six bytes.

---

## 15. Reconstructed CTCDC session

Binary-confirmed sequence for the path relevant to X4 control:

`CTCreateInstanceEx`
→ Creative component DLL resolution
→ `DllGetClassObject`
→ `IClassFactory::CreateInstance`
→ `ICTCDC::Initialize`
→ `ICTCDC::Open`
→ COM configure (`115200/8N1`, mask `0x05`, zero timeouts, purge, SETDTR)
→ `5A 03 00` GetMaximumPayloadSize
→ if unavailable: `whoareyou.MyApp8\r\n`
→ parse `whoareyou + seed4 + challenge32`
→ AES-256-GCM unlock reply
→ require `unlock_OK\r\n`
→ `SW_MODE1\r\n`
→ `5A 03 00` GetMaximumPayloadSize retry
→ `5A 09 01 02` GetFirmwareVersionString
→ `5A 26 01 05` QueryButtonsAvailable
→ normal session active
→ `ICTCDC::ExecuteCommand(1001, rawBuffer, rawSize)`
→ raw MIDAS command bytes

---

## 16. Existing ARM64 probe audit

Branch:

`poc/windows-arm64-usb-serial-ctcdc-init`

The following parts match the binary:

- event mask `0x05`
- `115200`
- `8N1`
- zero timeouts
- `PurgeComm(0x0F)`
- `SETDTR`
- initial bytes `5A 03 00`
- unlock hello `whoareyou.MyApp8\r\n`

Required corrections:

1. rename/reinterpret `5A 03 00` as `GetMaximumPayloadSize`, not firmware version
2. preserve unrelated DCB flag bits instead of forcibly clearing them
3. keep the current safe probe stopped before sending the cryptographic unlock reply until the first real X4 challenge is captured

The unlock algorithm is now statically recovered, but **no hardware challenge/runtime result is recorded yet**. The controlled Stage C observation should therefore remain the next hardware step before enabling the generated unlock response.

---

## 17. Evidence classification

### Binary-confirmed

All protocol bytes, function addresses, COM/API sequence, challenge layout, key construction, AES-256-GCM use, response layout, success string, software-mode command, post-session queries, and command-1001 raw passthrough behavior recorded above.

### Inference

The reason an already-responsive `GetMaximumPayloadSize` path can skip unlock/`SW_MODE1` is not assigned beyond the observed branch behavior. The code only proves that CTCDC treats the session as sufficiently responsive to continue.

### Hardware-confirmed runtime result

None added by this document.

The previous hardware result remains: naked COM and tested HID writes of the known Direct Mode bytes caused no physical X4 reaction.

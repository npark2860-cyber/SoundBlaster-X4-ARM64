# DEBUG HISTORY — ASIO WDF crash fingerprint across four native ARM64 variants

Date: 2026-09-03 KST

## Purpose

Consolidate the four kernel dumps produced by the native ARM64 WaveRT experiments and determine whether they represent separate failures or one repeated kernel failure path.

## Compared dumps

1. `090326-18031-01.dmp` — Stage A
2. `090326-16687-01.dmp` — Stage A1 repeated lifecycle
3. `090326-15921-01.dmp` — Stage A1 delayed-reopen variant
4. `090326-16234-01.dmp` — Windows SDK ABI baseline A0

## Header fingerprint

All four dumps are ARM64 kernel triage dumps and all four report the same bugcheck class and subtype:

- MachineImageType: `0xAA64`
- BugCheck: `0x10D` = `WDF_VIOLATION`
- Parameter 1: `0x5`
- Parameter 3: `0x1200`

Parameter 2 and Parameter 4 differ per runtime instance, as expected for instance-specific kernel object addresses.

Observed Parameter 2 values:

- Stage A: `0x19748551C768`
- Stage A1: `0x4BFA0D321FD8`
- Stage A1 delayed reopen: `0x36F477D0E6D8`
- SDK ABI baseline: `0x31F67D2BDAB8`

Microsoft defines bugcheck `0x10D`, Parameter 1 `0x5` as an incorrect framework-object-handle type being passed to a WDF framework method.

## Stack fingerprint

The four dumps were compared using module-relative addresses rather than absolute kernel addresses so ASLR does not obscure equality.

The crash context PC is identical in all four dumps:

- `ntoskrnl.exe + 0x25ACEC`

More importantly, the captured kernel stack contains the same module-relative WDF / USB Audio 2.0 pattern in every dump. Repeated entries include:

- `Wdf01000.sys + 0xAB000`
- `usbaudio2.sys + 0x1A038`
- `usbaudio2.sys + 0x1A040`
- `usbaudio2.sys + 0x18000`
- `ntoskrnl.exe + 0xA905A0`
- `Wdf01000.sys + 0xB2E70`
- `Wdf01000.sys + 0x1B3C0`

The exact relative-address fingerprint repeats across all four dumps despite different process binaries and different ASLR bases.

## Consequence

These are not four unrelated crashes.

They are the same `usbaudio2.sys` / WDF kernel failure path being triggered repeatedly.

This materially changes the diagnosis:

- repeated lifecycle is not sufficient to explain the crash, because the SDK baseline used one lifecycle and reached the same kernel fingerprint;
- RUN-time WaveRT half-buffer writes are not sufficient to explain it, because later crash variants removed those writes;
- hand-declared ARM64 structure size/offset mismatch is not sufficient to explain it, because the SDK baseline compiled with Microsoft SDK structures and compile-time ABI guards, yet reached the same fingerprint;
- adding reopen delay is not sufficient to eliminate it;
- the failing WDF object handle is a kernel framework handle. User-mode code does not directly pass a WDF object handle to `usbaudio2.sys`; rather, a user-mode KS/WaveRT sequence is triggering a bad internal kernel object transition/path.

Do not interpret this as proof that `usbaudio2.sys` is generally defective. It proves that the current user-mode sequence reproducibly drives this Windows USB Audio 2.0 stack into the same WDF object-type violation on this machine/device combination.

## Important logging note

The SDK baseline used CRT `fopen_s`/`fflush` logging. The absence of `x4-asio-sdk-abi-baseline.txt` after a bugcheck does not prove that `main()` was never reached. `fflush()` flushes the CRT stream to the operating-system cache but is not equivalent to `FlushFileBuffers()` durability across a kernel bugcheck. Therefore no call-site conclusion is drawn from the missing text file.

## Static Creative ASIO reference finding

The supplied x64 reference `CtU2As64.DLL` contains the `KSPROPSETID_RtAudio` GUID and explicit use of:

- property ID `6` — `KSPROPERTY_RTAUDIO_REGISTER_NOTIFICATION_EVENT`
- property ID `7` — `KSPROPERTY_RTAUDIO_UNREGISTER_NOTIFICATION_EVENT`

Static teardown code around the unregister path also coordinates an internal worker/event/thread before closing its worker handle. This is reference evidence that Creative performs additional synchronization around its WaveRT runtime teardown. It does not yet establish which synchronization step avoids the observed ARM64 `usbaudio2.sys` failure.

Microsoft documentation says registered WaveRT notification events must be unregistered after the pin is stopped and before the pin is closed. The current probes follow that documented high-level rule, so the remaining differential is finer-grained sequencing/synchronization rather than merely 'forgot to unregister'.

## Next action — static only first

Do not run another hardware executable yet.

Highest-value next work:

1. statically recover the exact Creative `CtU2As64.DLL` sequence for:
   - WaveRT buffer allocation
   - notification-event registration
   - worker-thread start
   - KS state transitions
   - worker-thread shutdown
   - notification-event unregister
   - pin/filter handle close
2. compare that sequence call-by-call against the native ARM64 prototype;
3. identify the smallest behavioral difference that is common to all crashing variants;
4. only then build one new ARM64 probe with durable `CreateFileW + WriteFile + FlushFileBuffers` checkpoints before every kernel-facing call.

Do not return to repeated lifecycle, buffer writes, ASIO COM, capture, 24-bit, or higher sample rates until this kernel-path differential is isolated.

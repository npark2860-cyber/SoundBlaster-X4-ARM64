# DEBUG HISTORY — B5 mux v2 second C2059 / KS header isolation

Date: 2026-09-04 KST

## Returned Actions failure

The ARM64EC productization build failed again at the same point:

```text
Windows Kits\10\Include\10.0.26100.0\um\cguid.h(33,18):
error C2059: syntax error: '__uuidof'
```

while compiling `driver_b5_arm64ec.cpp`.

Therefore the previous diagnosis that moving `#define private public` below project/SDK headers was sufficient was incomplete.

No DLL or runtime package was produced by this run. It provides no new hardware result.

## New source-level finding

Compared with the earlier ARM64EC B5 adapter that had compiled before the mux work, the mux adapter had added these headers directly to the COM/ASIO driver translation unit before ASIO/COM declarations:

- `winioctl.h`
- `ks.h`
- `ksmedia.h`

The mux v2 worker no longer needs those headers because it consumes the WaveRT engine through the public `notification_event()` / `process_signaled_notification()` / `stats()` API.

`ks.h` defines C++ GUID helpers around `__uuidof`, so placing KS headers before later COM GUID declarations is an unnecessary include-order hazard in this translation unit.

## Fix applied

B5 branch advanced to:

`exp/windows-arm64-asio-b5-capability-productization@9ae7ba97277ef2bfb11bb0dbce42f671ed20b20d`

Removed direct KS includes from both:

- `src/asio-arm64-stage-b0/driver_b5_arm64ec.cpp`
- `src/asio-arm64-stage-b5-classic/driver_b5_classic.cpp`

Kernel Streaming headers remain isolated in the WaveRT engine translation units where KS IOCTL/property definitions are actually used.

No runtime behavior, BUSY gate, pin creation, packet pairing, MMCSS policy or strict failure criterion was relaxed by this compile fix.

## Important remaining architectural note

The ARM64EC driver adapter still uses the historical shared-source architecture shim around `driver_b5.cpp` (`_M_ARM64` defined and `_M_ARM64EC` temporarily undefined) because the shared B5 source currently rejects ARM64EC directly.

Microsoft documents that ARM64EC normally defines `_M_AMD64`/`_M_ARM64EC`, not `_M_ARM64`. If the same `cguid.h` failure persists after KS header isolation, the next action is not another include-order micro-fix: make the B5 shared driver source ARM64EC-aware and remove the architecture macro shim entirely.

## Next action

Re-run `Build ASIO B5 Productization` once from current B5 HEAD.

Do not hardware-test unless compile/link, PE checks, mux-v2 marker checks and ZIP packaging all PASS.

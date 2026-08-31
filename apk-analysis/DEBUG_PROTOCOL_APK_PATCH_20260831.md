# Debug Protocol APK Patch — 2026-08-31

## Goal

Expose the existing `DebugProtocolFragment` in Creative Android App 2.11.08 Internal Beta without rebuilding resources or adding new UI.

## Source APK

SHA-256:

`d613d4203585c4d50716ef0814b8b935906229930d280f2dc96bb6d6eb0479c1`

## Confirmed navigation behavior

Dashboard click handling contains an explicit branch:

- `ModuleModel.id == 100` -> navigate to `DebugProtocolFragment`

The feature-key mapping also contains:

- ID `18` -> `show_direct_mode`
- ID `100` -> `show_debug_protocol`

The production module list creates Direct Mode models with ID `18`, while no normal production path creates a module with ID `100`.

## Minimal patch strategy

Reuse the existing Direct Mode dashboard entry as a temporary Debug Protocol entry.

Only the `ModuleModel` constructor ID value is changed:

`18 -> 100`

The `show_direct_mode` visibility check remains unchanged. This means a device which normally exposes Direct Mode still creates and displays the same dashboard item, but clicking that item follows the already-existing ID 100 navigation path into `DebugProtocolFragment`.

No resource, layout, navigation XML, BLE implementation, protocol builder, or fragment code is modified.

Seven DEX literal locations were changed in `Lf9/p0;->s` to cover the product-specific branches which construct the Direct Mode module. DEX SHA-1 header signature and Adler-32 checksum were then regenerated.

Patched `classes.dex` SHA-256:

`e550b9039fe250ff346778a6d8963232db0ff2a712b71d3388d585c0f78b6a20`

## APK packaging/signing

Because the original Creative signing key is unavailable, the test APK is signed with a new test certificate. It therefore cannot update an installed official Creative APK in place; the official package must be removed before installing the test build.

The test APK was rebuilt with stored entries aligned, including 16 KiB alignment for uncompressed native `.so` files, and then signed using APK Signature Scheme v2 with RSA PKCS#1 v1.5 / SHA-256 (`0x0103`).

Final APK SHA-256:

`f469085a36d29a523b25091ef49ceb3d2f206eed12dae0be5ecfc7a13f38a67e`

## Verification performed

- APK v2 signing block parsed successfully.
- RSA signature over v2 `signed data` verified successfully.
- APK content digest recomputed and matched the signed digest.
- ZIP CRC verification passed for all entries.
- Patched `classes.dex` SHA-256 matched the expected value.
- All stored entries passed alignment checks; uncompressed native `.so` entries are 16 KiB aligned.

## Expected test flow

1. Install the patched Creative APK.
2. Connect to Sound Blaster X4 normally.
3. Open the dashboard item which is visually still the Direct Mode entry.
4. The item should now open the existing `DebugProtocolFragment` instead of performing the original Direct Mode action.
5. Use the fragment only for controlled protocol experiments.

## Scope warning

This build is a research/test build. The Direct Mode dashboard entry is intentionally hijacked as the Debug Protocol entry; this is not intended as a distributable replacement for the Creative App.

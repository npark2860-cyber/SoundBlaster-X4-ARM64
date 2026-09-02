# Sound Blaster X4 Windows ARM64 HID Caps Diagnostic

Read-only diagnostic for the X4 `MI_00` HID interface.

It does **not** send HID output reports or feature reports and does not change device state.

## What it dumps

For the X4 HID interface (`VID_041E`, `PID_3278`, `MI_00`) it records:

- HID device path and instance ID;
- VID/PID/version and product/manufacturer/serial strings when available;
- top-level HID Usage Page / Usage;
- input/output/feature report byte lengths;
- button caps for Input / Output / Feature reports;
- value caps for Input / Output / Feature reports;
- link collection tree, including each collection's Usage Page / Usage.

The purpose is to determine whether the Windows USB-local control path exposes Creative/vendor-defined HID feature reports or whether `MI_00` is only standard Consumer Control HID.

## Run

Connect the Sound Blaster X4 by USB, then run:

```powershell
.\x4-hid-diag.exe
```

It prints the report and writes:

`x4-hid-diag.txt`

Send that TXT back for analysis.

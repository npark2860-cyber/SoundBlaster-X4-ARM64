# Sound Blaster X4 Windows ARM64 HID Output PoC

This is a narrowly scoped hardware-validation PoC for the X4 `MI_00` HID interface.

The read-only HID diagnostic established:

- VID/PID: `041E:3278`
- interface: `MI_00`
- HID Usage Page: `0x000C` Consumer
- Report ID: `0`
- OutputReportByteLength: `65`
- Output payload capacity: 64 bytes
- Feature reports: none

The PoC therefore constructs one 65-byte output report:

- byte 0: Report ID `00`
- bytes 1..6: the previously hardware-validated Direct Mode command
- remaining bytes: zero padding

Commands:

- OFF payload: `5A 39 03 00 05 00`
- ON payload: `5A 39 03 00 05 01`

## Usage

Default transport is `HidD_SetOutputReport`:

```powershell
.\x4-hid-output-poc.exe off
.\x4-hid-output-poc.exe on
```

Explicitly:

```powershell
.\x4-hid-output-poc.exe off setoutput
.\x4-hid-output-poc.exe on setoutput
```

If that API accepts the report but the hardware does not react, test the interrupt-output path separately:

```powershell
.\x4-hid-output-poc.exe off write
.\x4-hid-output-poc.exe on write
```

The program does not automatically fall back from one method to another so that hardware results remain attributable to one transport method.

An API success message only means Windows accepted the 65-byte HID report. The physical X4 state is the actual validation result.

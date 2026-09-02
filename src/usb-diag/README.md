# Sound Blaster X4 Windows ARM64 USB Diagnostic

This is a read-only diagnostic for the **USB-local Windows control path** of Sound Blaster X4.

It does not use Bluetooth and it does not send any command to the device.

## What it collects

The tool enumerates present Windows PnP nodes and keeps nodes matching Creative USB vendor ID `VID_041E`, `Sound Blaster X4`, or `SB1815`, plus their descendants.

For each matched node it records:

- device instance ID, including `MI_xx` when Windows exposes a USB composite interface;
- parent device instance ID;
- friendly name and device description;
- setup class and class GUID;
- bound Windows service/driver key;
- hardware IDs and compatible IDs;
- location information and location paths;
- Config Manager devnode status/problem code.

It also enumerates matching standard USB-device and HID device-interface paths.

This is intended to distinguish the X4's audio interfaces from any HID, WinUSB, or Creative vendor-specific control interface before implementing writes.

## Run

Connect the X4 to the PC by USB, then run:

```powershell
.\x4-usb-diag.exe
```

The program prints the report and creates:

`x4-usb-diag.txt`

Send that TXT back for analysis. Administrator privileges should not be required for enumeration.

## Safety / scope

This build is enumeration-only. It does not open endpoints for output, send USB control transfers, HID reports, or write vendor commands.

# Sound Blaster X4 Windows ARM64 USB Serial PoC

Minimal native Windows ARM64 proof-of-concept for testing the X4's USB-local control path exposed as the Creative CDC/serial interface.

## Source of the target port

The user's X4 USB diagnostic identified:

- `USB\\VID_041E&PID_3278&MI_01`
- Windows service: `usbser`
- current friendly name: `USB Serial Device (COM3)`

The executable auto-detects the current COM port belonging to that exact `MI_01` hardware ID, so it is not tied to COM3 if Windows later assigns another number.

## Commands under test

These six-byte frames were already validated against the physical X4 through Creative's Android Debug Protocol path:

- Direct Mode OFF: `5A3903000500`
- Direct Mode ON: `5A3903000501`

This PoC tests whether Windows' local USB serial interface accepts the same frame directly.

## Run

Connect the X4 by USB, then run:

```powershell
.\\x4-serial-poc.exe off
.\\x4-serial-poc.exe on
```

An explicit port can be supplied for diagnosis:

```powershell
.\\x4-serial-poc.exe off COM3
```

The program opens the port with Win32 `CreateFileW` and writes exactly six raw bytes with `WriteFile`. It does not add a delimiter, newline, BLE/GATT framing, or another protocol wrapper.

## Interpretation

- If the X4 switches Direct Mode, `MI_01` is confirmed as a usable Windows USB-local transport for this command family.
- If six bytes are successfully written but the X4 does not react, the USB serial path likely requires additional framing/session setup or a different Windows interface. That result should be treated as a negative transport test, not as evidence that the already hardware-validated `5A39...` command itself is wrong.

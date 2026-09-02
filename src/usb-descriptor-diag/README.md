# Sound Blaster X4 raw USB descriptor diagnostic

Read-only Windows ARM64 diagnostic for Sound Blaster X4 (VID 041E, PID 3278).

The tool enumerates USB hubs, finds the X4 by its device descriptor, and asks the parent hub for the raw USB configuration descriptor using `IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION`.

It does **not** send any vendor command or audio control write.

Output: `x4-usb-descriptor.txt` in the current directory.

The dump includes:
- hub path and port
- USB device/configuration information
- every raw descriptor in the active configuration
- interface class/subclass/protocol
- class-specific Audio descriptors (`0x24`)
- UAC2 Extension Unit candidates
- vendor-class (`0xFF`) interface count

Run:

```powershell
.\x4-usb-descriptor-diag.exe
```

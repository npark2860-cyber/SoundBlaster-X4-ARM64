# Sound Blaster X4 Windows Audio Topology Diagnostic

Read-only Windows ARM64 diagnostic for the Sound Blaster X4 USB Audio 2.0 path.

It enumerates Windows MMDevice endpoints matching `Sound Blaster X4` / `VID_041E&PID_3278`, activates `IDeviceTopology`, and dumps:

- endpoint flow/state
- topology connectors
- part names and local/global IDs
- part subtype GUIDs
- connector type/data flow
- control-interface names and IIDs
- incoming/outgoing topology links

No USB, HID, serial, audio, or vendor control write is performed.

Run:

```powershell
.\x4-audio-topology-diag.exe
```

The program saves `x4-audio-topology.txt` in the current directory. Upload that TXT for analysis.

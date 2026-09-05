# X4 MMDevice Endpoint Association Probe (ARM64)

Read-only Windows Core Audio probe used after the Stage A0 APO COM and usbaudio2 attachment gates.

It enumerates MMDevice endpoints whose friendly name contains `Sound Blaster X4` and reports:

- endpoint ID;
- render/capture data flow;
- endpoint state;
- `PKEY_AudioEndpoint_Association`;
- `PKEY_AudioEndpoint_FormFactor`.

The association value is expected to be the KS pin-category GUID used by Windows to associate an endpoint with a KS pin. The form factor distinguishes endpoint roles such as Speakers, Headphones, Microphone and SPDIF.

This probe performs no property writes, registry writes, CTCDC commands or driver installation.

Run `RUN-MMDEVICE-ENDPOINT-PROBE.cmd` from the built artifact. It writes `X4_MMDEVICE_ENDPOINT_REPORT.txt` in the same directory.

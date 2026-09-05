# X4 ARM64 usbaudio2 attachment probe

This is a **read-only** Windows ARM64 probe used before any live APO package installation.

Target X4 audio interface:

`USB\VID_041E&PID_3278&MI_03`

## What it reads

- present X4 audio devnode identity/class/service/driver key;
- `FX` / `EP` registry subtrees from the devnode hardware/software keys when readable;
- present `KSCATEGORY_AUDIO` interfaces whose devnode ancestry reaches the X4 MI_03 interface;
- present `KSCATEGORY_TOPOLOGY` interfaces with the same ancestry;
- interface-registry `FX` / `EP` subtrees when present;
- KS pin count/category/dataflow/communication by GET-only `IOCTL_KS_PROPERTY` requests;
- MMDevice endpoints plausibly associated with Sound Blaster X4.

## What it never does

- no registry writes;
- no endpoint-property writes;
- no CTCDC command;
- no driver install/uninstall;
- no APO registration;
- no `regsvr32`;
- no AudioDG restart;
- no device state change.

The KS interface is opened with `GENERIC_READ` only. All KS requests use `KSPROPERTY_TYPE_GET`.

## Goal

Before converting the `.inx.review` package templates into a real Windows 11 APO package, prove the actual Microsoft `usbaudio2` attachment model on the ARM64 test machine:

1. which devnode/interface owns the X4 FX/EP property stores;
2. whether FX slot numbering already exists;
3. which KS pin categories correspond to Speaker, Headphone and Microphone;
4. what exact association values a future pass-through extension package must use.

## Run

Use `RUN-ATTACHMENT-PROBE.cmd` from the built artifact. It writes:

`X4_USBAUDIO2_ATTACHMENT_REPORT.txt`

Return that text report for analysis before any installation step.

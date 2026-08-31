# Runtime Validation — Sound Blaster X4

This is the minimum runtime test required before implementing a Windows ARM64 control client.

## Goal

Validate one known X4 command end-to-end on real hardware:

**Direct Mode OFF ↔ ON**

Static analysis already identifies the X4 BLE transport as:

- BLE device name: `Control for SB1815`
- Service: `b7860001-11b8-b681-6343-5a6c2286633f`
- Write characteristic: `b7860002-11b8-b681-6343-5a6c2286633f`
- Notify/read characteristic: `b7860003-11b8-b681-6343-5a6c2286633f`
- CCCD: `00002902-0000-1000-8000-00805f9b34fb`

## Known Direct Mode Command

Command ID: `0x39`

Raw feature payload:

- OFF: `00 05 00`
- ON: `00 05 01`

Expected legacy MIDAS BLE writes:

- OFF: `6A 39 03 00 00 05 00`
- ON: `6A 39 03 00 00 05 01`

If the negotiated runtime protocol uses extended MIDAS framing, the write will instead begin with `5C 39 03 00 00 05 00/01` and include session-dependent suffix bytes.

## Android Capture Procedure

1. Pair/connect the Sound Blaster X4 to the Android Creative App.
2. In Android Developer options, enable **Bluetooth HCI snoop log**.
3. Disable and re-enable Bluetooth after enabling the option if required by the device.
4. Open Creative App and connect to the X4.
5. Do not change any other X4 setting during the capture window.
6. Toggle **Direct Mode exactly once**.
7. Wait several seconds, then stop interacting with the app.
8. Produce an Android bug report with `adb bugreport` or the system bug-report function.
9. Preserve the resulting ZIP without editing it.

The original Creative APK does not need to be included with the capture.

## What to Upload for Analysis

Preferred input:

- the complete Android bug-report ZIP from the capture window

If a standalone Bluetooth snoop file is available, it is also sufficient:

- `btsnoop_hci.log`
- `btsnoop_hci.log.last`
- equivalent Bluetooth HCI snoop capture

## Validation Criteria

The runtime test passes when all of the following are demonstrated:

1. Connection is to `Control for SB1815`.
2. The X4 exposes or is accessed through service `b7860001-11b8-b681-6343-5a6c2286633f`.
3. Direct Mode causes an ATT/GATT write to `b7860002-11b8-b681-6343-5a6c2286633f`.
4. The payload contains command `0x39` and Direct Mode selector `0x05`.
5. The final framing mode is identified as either legacy `0x6A` or extended `0x5C`.
6. The device state actually changes in response to that write.

## After Validation

Only after this test passes:

1. freeze the confirmed runtime frame format in `PROTOCOL.md`;
2. implement a minimal Windows ARM64 BLE proof-of-concept;
3. first proof-of-concept function: connect to `Control for SB1815` and toggle Direct Mode;
4. do not begin the full UI until the proof-of-concept can reproduce the command independently.

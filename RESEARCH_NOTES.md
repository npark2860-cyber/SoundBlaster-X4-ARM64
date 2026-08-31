# Research Notes

## 2026-08-31 — Initial APK static triage

### Input

Android Creative App APK supplied for analysis.

SHA-256:

`d613d4203585c4d50716ef0814b8b935906229930d280f2dc96bb6d6eb0479c1`

### Confirmed findings

1. X4 identifiers:
   - `Sound Blaster X4`
   - `Accent2`
   - `SB1815`
   - `Control for SB1815`

2. X4/SB1815 resource names:
   - `/SB1815_metadata.json`
   - `SB1815/SB1815_external.json`
   - `SB1815/SB1815_internal.json`
   - `SB1815/SB1815_release.json`
   - `SB1815_equalizer`

3. BLE/GATT configuration keys:
   - `keyServiceUUID`
   - `keyReadCharacteristicUUID`
   - `keyWriteCharacteristicUUID`

4. The APK contains ARM64 native libraries in `lib/arm64-v8a/`.

---

## 2026-08-31 — X4 GATT UUID trace

### Connection selection

The BLE connection implementation receives a `bleName`, invokes the device UUID selector, then calls `BluetoothDevice.connectGatt(...)`.

The selector contains three main UUID families. A group of newer devices is routed to alternate UUID sets by matching device-name tokens. The X4 BLE name `Control for SB1815` is not present in those exception branches and therefore uses the selector's default family.

### Default family selected by X4

- Service: `b7860001-11b8-b681-6343-5a6c2286633f`
- Characteristic B: `b7860002-11b8-b681-6343-5a6c2286633f`
- Characteristic C: `b7860003-11b8-b681-6343-5a6c2286633f`
- CCCD: `00002902-0000-1000-8000-00805f9b34fb`

### Characteristic direction proof

The GATT callback path resolves the selected service and both characteristics.

- Characteristic B is used by `setValue(...)` followed by `BluetoothGatt.writeCharacteristic(...)` after descriptor setup. Therefore B is the **write characteristic**.
- Characteristic C is registered using `setCharacteristicNotification(...)`, and `onCharacteristicChanged(...)` explicitly compares incoming characteristic UUIDs against C. Therefore C is the **read / notify characteristic**.

### Result

The X4 GATT map is now promoted from candidate to **Confirmed**:

- Service: `b7860001-11b8-b681-6343-5a6c2286633f`
- Write: `b7860002-11b8-b681-6343-5a6c2286633f`
- Read/Notify: `b7860003-11b8-b681-6343-5a6c2286633f`
- CCCD: `00002902-0000-1000-8000-00805f9b34fb`

Public Windows device-enumeration records for systems containing `Control for SB1815` independently show the `b7860001-...` BLE service, consistent with the APK trace.

### Next action

Trace one simple X4 setting to the exact command bytes delivered to the write characteristic. Do not begin Windows ARM64 UI/client implementation until at least one command can be independently reproduced.

# X4 GATT UUID Static Trace — 2026-08-31

APK SHA-256:

`d613d4203585c4d50716ef0814b8b935906229930d280f2dc96bb6d6eb0479c1`

## Relevant obfuscated classes

Names below are from this specific APK build and may change in later versions.

- `Lxc/a;` — BLE connection state / selected UUID holders
- `Lxc/b;` — `BluetoothGattCallback` implementation
- `Lxc/c;` — UUID constants and device-name UUID selector

## X4 route

The application maps the X4 to BLE name:

`Control for SB1815`

`Lxc/a;->b(address, bleName)` calls the UUID selector `Lxc/c;->g(bleName)` immediately before `BluetoothDevice.connectGatt(...)`.

`Lxc/c;->g(...)` has explicit alternate branches for other model families. `Control for SB1815` matches none of them and falls through to the default assignment:

- selected service (`Lxc/a;.A`) ← `b7860001-11b8-b681-6343-5a6c2286633f`
- selected characteristic (`Lxc/a;.B`) ← `b7860002-11b8-b681-6343-5a6c2286633f`
- selected characteristic (`Lxc/a;.C`) ← `b7860003-11b8-b681-6343-5a6c2286633f`
- selected descriptor (`Lxc/a;.D`) ← `00002902-0000-1000-8000-00805f9b34fb`

## Direction proof

`Lxc/b;->onServicesDiscovered(...)`:

1. resolves service using selected `.A`
2. resolves two characteristics using `.B` and `.C`
3. enables notifications and writes the CCCD `.D`

`Lxc/b;->onDescriptorWrite(...)` then obtains characteristic `.B`, calls `setValue(...)`, and calls `BluetoothGatt.writeCharacteristic(...)`.

Therefore:

**`.B` = write = `b7860002-...`**

`Lxc/b;->onCharacteristicChanged(...)` compares the incoming characteristic UUID to selected `.C` before parsing the returned value.

Therefore:

**`.C` = read/notify = `b7860003-...`**

## Confirmed map

| Role | UUID |
|---|---|
| Service | `b7860001-11b8-b681-6343-5a6c2286633f` |
| Write | `b7860002-11b8-b681-6343-5a6c2286633f` |
| Read / Notify | `b7860003-11b8-b681-6343-5a6c2286633f` |
| CCCD | `00002902-0000-1000-8000-00805f9b34fb` |

## Next trace

Follow a user-visible X4 setting into the packet builder and capture the exact `byte[]` written to the Write characteristic.

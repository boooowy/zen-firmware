# ZEN telemetry protocol v1

Live keyboard state streamed from the ZEN central half to a companion app
(zen-hud). This document is the contract: the firmware in `src/zen_telemetry*.c`
and the macOS decoder are both written against it.

All multi-byte integers are **little endian**.

## Transport

A custom GATT service on the central (right) half, riding the same BLE
connection macOS already holds for HID. No cable, no second pairing, and no
Input Monitoring permission on the host.

| | UUID |
|---|---|
| Service | `47c59b6d-048a-4fa7-921f-955c966a2c38` |
| `events` characteristic | `47c59b6e-048a-4fa7-921f-955c966a2c38` |
| `snapshot` characteristic | `47c59b6f-048a-4fa7-921f-955c966a2c38` |

- `events`: **Notify** only. Notifications are used rather than indications so a
  burst of typing can never block the link waiting for an ack.
- `snapshot`: **Read + Notify**.
- Both CCC descriptors require an encrypted (bonded) link.

Enabled by `CONFIG_ZEN_TELEMETRY=y` — see the `zen-telemetry` snippet and the
`zen_right_trackball_pmw3610_central_telemetry` build in `build.yaml`.

### Payload size

The firmware assumes the worst case of **ATT MTU 23**, so a frame is at most
**20 bytes**. When the negotiated MTU is larger the firmware packs more records
per notification (up to 64 bytes). A reader must handle any length from 3 bytes
up to 64. Data length extension does not change this — the ATT payload ceiling
is set by the MTU, not by the link layer PDU size.

## `events` frame

```
byte 0   proto_ver      always 1 for this document
byte 1   frame_seq      increments per frame, wraps at 256
byte 2+  records        back-to-back, no padding
```

A gap in `frame_seq` means frames were lost. The reader should re-read
`snapshot` to resynchronise rather than try to reconstruct what it missed.

Records are self-describing: the **type byte alone determines the length**, so a
reader never needs a length field and can always skip forward. An unknown type
byte means the stream is desynchronised — discard the rest of the frame.

### `0x01` POSITION — 5 bytes

Physical key press or release, before the keymap resolves it to anything.

```
0  0x01
1  flags    bit0 = pressed, bit1 = left half (peripheral)
2  position 0..49 on ZEN
3  t16      low 16 bits of the firmware uptime in ms
4
```

`position` is unique across both halves, so `bit1` is redundant on ZEN. It is
sent anyway because it comes straight from the ZMK event source and stays
correct if the layout ever changes.

`t16` wraps every 65.536 s. It exists so the host can measure intervals between
records with firmware timing rather than BLE arrival timing — necessary for
reproducing combo windows.

> **Combos:** ZMK captures position events while a combo is a candidate and
> re-raises them if it does not fire. This telemetry is emitted *before* that
> capture, so the host sees the raw press immediately, and sees a **duplicate**
> press for the same position if the combo fell through. See "Combo detection"
> below.

### `0x02` LAYER — 5 bytes

Sent on every layer change, carrying the whole state rather than a delta.

```
0  0x02
1  layer_bitset  uint32, bit N set = layer N active
2
3
4
```

Bits are indexed by ZMK **layer id**. Ids equal keymap order unless layers get
reordered in ZMK Studio.

### `0x03` KEYCODE — 8 bytes

What the keymap actually produced. Useful for showing real output and for
corroborating combo firing.

```
0  0x03
1  flags       bit0 = pressed
2  usage_page  low byte (0x07 keyboard, 0x0C consumer)
3  keycode     uint16, HID usage id
4
5  implicit_modifiers
6  t16
7
```

### `0x04` MODS — 2 bytes

```
0  0x04
1  modifiers   HID modifier bitmask, LCTRL=bit0 .. RGUI=bit7
```

Emitted only when the mask changes.

## `snapshot` — 20 bytes

The authoritative full state. Sent when a host subscribes, when device status
changes (battery, endpoint, split connection), and after any dropped or failed
frame. It is never sent periodically — that would cost battery for nothing.
A host may also read it at any time.

```
byte  0      proto_ver
byte  1      flags        bit0 = split peripheral connected
byte  2..5   layer_bitset uint32
byte  6..12  pressed_positions  bitset, 7 bytes, position N = byte N/8 bit N%8
byte  13     modifiers
byte  14     battery_central     percent, 0 if unavailable
byte  15     battery_peripheral  percent, 0 if unavailable
byte  16     endpoint     low nibble: 0 = USB, 1 = BLE
                          high nibble: active BLE profile index
byte  17     highest_active_layer
byte  18..19 dropped_events  uint16, saturating count since boot
```

`dropped_events` increasing means the host is not keeping up or the link
stalled; the HUD can surface it, but the snapshot itself already repairs the
state.

## Combo detection (host side)

The firmware sends no combo records. ZMK v0.3 has no public combo event —
candidates, timeouts and firing all live in static state inside
`app/src/combo.c` — so exposing them accurately would mean forking ZMK.

Instead the host reproduces combo state from the combo definitions in the
keymap (`key-positions`, `layers`, `timeout-ms`, `require-prior-idle-ms`) plus
the POSITION and KEYCODE records. This keeps combo tuning on the macOS side,
where it needs no reflash.

What this can and cannot do:

- **Candidate ("waiting") display is accurate.** The host has the same inputs
  ZMK does at that moment.
- **Firing is inferred, not reported.** Two signals corroborate it:
  1. **No duplicate press.** ZMK re-raises captured position events when a combo
     does *not* fire, so a second press record for the same position within the
     combo window means it fell through. Absence means it likely fired. This
     depends on module listeners running before ZMK core listeners; verify on
     hardware before relying on it.
  2. **Keycode correlation.** `switch-ime` emits `LC(SPACE)` and `haifun` emits
     `MINUS`, so a matching KEYCODE record right after the candidate resolves
     confirms the firing. `bt_clear` emits no keycode and cannot be confirmed
     this way.

If exact reporting is ever needed, the smallest honest fix is to point
`config/west.yml` at a ZMK fork that raises a `combo_triggered` event — a few
lines in `combo.c` — and add a `0x05` record type here.

## Versioning

`proto_ver` is the first byte of every frame and of the snapshot. Bump it for
any change to an existing record layout **and for any new record type** — since
lengths are derived from the type byte, an old reader that meets an unknown type
has to discard the rest of the frame, losing the records behind it. A reader
should refuse to decode a `proto_ver` it does not know and tell the user to
update, rather than guess.

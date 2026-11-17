# AULA device protocols

This document notes the protocol details for AULA wireless peripherals: the Compx-based 2.4 GHz keyboards (F75, F87 Pro, F99) supported by the application, and initial capture findings for the unsupported AULA SC311 wireless mouse.

## Keyboards (Compx receiver 3554:FA09)

AULA wireless keyboards (F75, F87 Pro, F99, and related Compx-based models) communicate over a 2.4 GHz USB receiver with vendor ID `3554` and product ID `FA09`. The receiver exposes a 20-byte vendor collection on usage page `FF02` / usage `0002`.

Every AULA keyboard ships with its own receiver and they all enumerate with `3554:FA09`. Multiple receivers can be plugged in at once; each receiver is queried individually and keyed by its parent device instance tag (e.g. `InstanceTag(c->path)`), allowing each connected keyboard to maintain its own identity in the UI.

### Frame layout

Commands and responses use 20-byte HID reports with Report ID `0x13`:

- **Byte [0]**: Report ID (`0x13`)
- **Byte [1]**: Command byte in requests; in replies, the command byte matches the request with its top bit masked (`(reply[1] & 0x7F) == cmd`)
- **Byte [2]**: Command argument (e.g. `0x01` for link status)
- **Bytes [3..18]**: Payload parameters (zero-padded when unused)
- **Byte [19]**: 8-bit sum checksum (`sum(bytes[0..18]) & 0xFF`)

The receiver also pushes unrelated `0x13` frames (such as `0x0A` status broadcasts), so the Report ID alone does not identify the reply. The driver validates both the command byte and the trailing sum checksum.

### Commands

- **`0x07` Link status**:
  - Sent first by the OEM software.
  - Reply byte `[5]`: `1` when a keyboard is linked to this receiver, `0` when the receiver is empty (observed on a receiver with no keyboard paired; a switched-off keyboard is untested).
  - Empty receivers are detected immediately and skipped, avoiding unnecessary battery queries.
- **`0x4A` Battery status**:
  - Reply byte `[5]`: Battery level percentage (`1`–`100`).
  - Reply byte `[6]`: State flag (`0x01` on battery / discharging, `0x10` with the cable plugged in / charging).
  - The first battery request can go unanswered while the 2.4 GHz link wakes up, so the driver retries `0x4A` once before applying a backoff timer.
- **`0x05` Model ID**:
  - Reply byte `[10]`: Keyboard model identifier.

### Observed replies

- **Link status — keyboard linked (`0x07`)**:
  ```text
  13 07 01 00 01 01 ... 1D
  ```
  Byte `[5]` is `0x01` (linked).

- **Link status — empty receiver (`0x07`)**:
  ```text
  13 07 01 00 01 00 ... 1C
  ```
  Byte `[5]` is `0x00` (empty receiver).

- **Battery status — 88 %, on battery (`0x4A`)**:
  ```text
  13 4A 01 00 02 58 01 ... B9
  ```
  Byte `[5]` is `0x58` (88 %), byte `[6]` is `0x01` (battery).

- **Model ID (`0x05`)**:
  ```text
  13 05 01 00 0A 03 00 00 00 01 0B 01 ... 33
  ```
  Byte `[10]` is `0x0B` (model ID).

---

## AULA SC311 mouse (not supported yet)

The AULA SC311 is a wireless mouse that uses a completely different protocol from the Compx keyboards and is not supported yet.

### Hardware identification

| Connection | VID:PID | Product string | Notes |
|---|---|---|---|
| 2.4 GHz receiver | `A8A5:2255` | `"USB Receiver"` | Serial number prefix: `D1K_...` |
| USB cable | `A8A4:2255` | `"USB MOUSE"` | Appears only while the cable is plugged in |

### Collections

Both the wireless receiver (`A8A5:2255`) and the wired cable (`A8A4:2255`) expose the following vendor collections:

- `FF06:0002` (64 bytes input / 64 bytes output)
- `FF05:0002` (64 bytes input)
- `FF01:0010` on interface 2 (65 bytes input / 65 bytes output)
- `FFA5:0088`

### Protocol behavior

- The mouse ignores the Compx `0x13` commands.
- It pushes unsolicited 65-byte frames on collection `FF01:0010` (interface 2) approximately every 1–2 minutes on the receiver.
- None were seen on the receiver while charging over the cable.

### Captured frames (first 16 bytes)

Receiver (unsolicited push frames, remaining bytes are all zero):

```text
AA FA AE 36 38 01 01 01 D0 2B
AA FA AE 37 38 01 01 01 D0 2C
AA FA AE 38 38 01 01 01 D0 2D
```

Cable while charging:

```text
AA FA 00 23 38 00 00 00 D0 64 00 00 6D 2E D0 5A
```

### Open questions

- **Which byte is the level**:
  - Byte `[9]` reads `43`–`45` (`0x2B` = 43, `0x2C` = 44, `0x2D` = 45) on battery and `100` (`0x64` = 100) on the cable.
  - Byte `[3]` reads `54`–`56` (`0x36` = 54, `0x37` = 55, `0x38` = 56) on battery and `35` (`0x23` = 35) on the cable.
- **Checksum**: The checksum or frame validation algorithm across the 65-byte payload is unknown.
- **Query command**: Whether an active query command exists to request battery status on demand.

### Help needed

Ask owners of the AULA SC311 mouse for a USB capture (e.g. Wireshark with USBPcap) of the vendor software communicating with the mouse, or packet readings captured at known battery levels.

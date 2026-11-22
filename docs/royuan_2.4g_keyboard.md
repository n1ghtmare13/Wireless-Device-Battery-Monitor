# ROYUAN `3151:4011` 2.4 GHz keyboard dongle (MonsGeek)

## Scope and hardware validation

Measured on a **ROYUAN `3151:4011`** dongle, product string `ROYUAN MonsGeek 2.4G Wireless
Keyboard`, paired with a MonsGeek M5W board, on Linux (CachyOS, kernel 7.3.4).

`PollRongyuan()` recognises this product ID, but on this firmware its reply layout does not
hold. The driver reads byte 4 as *keyboard offline*; this dongle sets byte 4 to `1` while the
board is connected and reporting a live level, so every keyboard slot would be skipped and
the device would stay silent. The level itself sits exactly where the driver expects it
(byte 2). Nothing about this dongle's protocol is new; only the status byte differs.

```
$ iot_driver dongle status
Dongle Status (F7)
==================
  Has Response:   No
  Battery Level:  52%
  Charging:       Yes
  RF Ready:       Yes
```

The 52% agrees with the reference reader's own history for the same board, so the level decode
is sound. The `Charging` field is documented below as unreliable on this firmware and is not
proposed for use.

## Protocol

The dongle answers the `F7` *get dongle status* command on its vendor collection
`FFFF:0002` — report ID `0`, a **64-byte feature report** (the only report of that
collection, so it is the whole interface). The request is a zero-filled 64-byte buffer with
the command in the first payload byte and its checksum in the eighth:

```text
00 F7 00 00 00 00 00 00 08 00 ... 00     (65-byte Windows feature report, report ID 0)
                          ^^ checksum: 0xFF - sum(00 F7 00 00 00 00 00) = 0x08
```

Reply, 16 bytes of payload with the report ID still in front:

```text
00 34 00 01 01 01 01 00 00 00 00 00 00 00 00 00
```

| Byte | Meaning | This capture |
|---|---|---|
| 0 | report ID | `00` |
| 1 | has_response | `0x34` |
| 2 | battery level, 0–100 | `0x34` = **52%** |
| 3 | reserved | `00` |
| 4 | not an offline flag on this firmware | `01` |
| 5 | constant | `01` |
| 6 | rf_ready | `01` |

Byte 1 is not a bool on this firmware: it carried the same value as the percentage
(`0x34`), so it must not be tested as *is nonzero* for anything but "the dongle answered".

## Charging state is not usable

The charging byte does not describe the board. Measured against the reference reader's
sample history, the level sat at 52% for every minute of a session with no cable attached,
while the `F7` reply reported `Charging: Yes` on each poll. The board also stops answering
the battery query while its cable is in, so a plugged-in board reads as a frozen previous
value rather than as charging.

Practical rule for this dongle: **use the level only.** A charge event can be inferred from
the level rising between polls, never from the charging byte, and a board on its cable must
not be presented as charging.

A sleeping board's dongle reports **0%**, which is "no frame from the board", not an empty
cell. Treat `0` as unknown. The board stops transmitting long before its cell is flat.

## Requests sent

Only the `F7` status query, which is what the vendor's own application sends. It is a read:
no settings, pairing, lighting, keymap, macro or firmware command is sent, and this dongle
has no battery command of its own to fall back on.

## Tests

`tests/receiver_frames_test.cpp` decodes the captured frame above, checks the request
checksum and asserts that byte 4 does not affect the reading, without any hardware:

```
g++ -std=c++17 -O2 -Wall -Wextra tests/receiver_frames_test.cpp -o build/receiver_frames_test
./build/receiver_frames_test
```

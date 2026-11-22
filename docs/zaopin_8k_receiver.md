# Kysona-protocol receivers on Compx `3554:F527` (Zaopin 8K)

## Scope and hardware validation

This note records the measured frames of a **Compx 2.4 GHz 8K receiver**, `3554:F527`,
product string `Compx 2.4G Wireless 8K Receiver`, `bcdDevice` `0202` (firmware v2.02),
shipping with a **Zaopin Z2 Mini** mouse. It is the same read-only Kysona protocol that
`src/drivers/kysona.cpp` already speaks; nothing new is sent, and no driver change is
requested.

Measured on Linux (CachyOS, kernel 7.3.4) against the receiver's vendor collection
`FF02:0002`, 17-byte output and 17-byte input reports, report ID `08`. The other collections
of this receiver (`FF05:0000`, `FF03:0000`, `FF04:0002`, `0001:0002`) were enumerated and
deliberately left alone.

```
$ wireless-battery --list      # root, hidraw nodes are 0600 by default
Compx 2.4G Wireless 8K  [2.4 GHz receiver]  45%, 3837 mV   (kysona:3554:F527)
```

The level was cross-checked against an independent reader on the same receiver in the same
minute (45%, 3833–3837 mV), so the decode below is not a single-source reading.

## Protocol

Exactly the Kysona framing already implemented in `src/drivers/kysona.cpp`:

| Byte | Meaning |
|---|---|
| 0 | `08`, report ID |
| 1 | command: `03` online status, `04` battery |
| 2–15 | zero |
| 16 | `0x55 - sum(bytes 0..15)`, modulo 256 |

Reply (17 bytes, report ID `08` kept at byte 0):

| Byte | Meaning |
|---|---|
| 0 | `08` |
| 1 | command echo |
| 5 | status, `02` = connected |
| 6 | percent, 1–100 |
| 7 | charging, 1 = charging |
| 8–9 | cell voltage, big-endian, millivolts |

Example captured reply, 95% discharging at 4080 mV:

```text
08 04 00 00 00 02 5F 00 0F F0 00 00 00 00 00 00 E9
                                              ^^^ 0x55 - sum(0x08 0x04 .. 0x00) = E9
```

Checksum check: `sum(bytes 0..15) = 0x6C`, `0x55 - 0x6C = 0xE9`, matching byte 16.

## Behaviour on this firmware

- The mouse answers `03` then `04` only while it is awake. A sleeping mouse returns nothing,
  which is why the driver keeps the previous value; the receiver itself stays reachable.
- `03` is needed first. A bare `04` on an idle receiver did not answer in testing, so the
  two-step wake in `kysona.cpp` is the correct shape for this dongle.
- The receiver reports millivolts consistently with its percentage: across the reference
  reader's stored samples the level runs 45% at 3829–3854 mV, 50% at 3850–3896 mV and 100%
  at 4126–4151 mV, so the two fields agree and millivolts can be shown as they are.
- Measured here (Zaopin Z2 Mini): 45% at 3837 mV, 95% at 4080 mV.
- Charging transitions, cable mode and Bluetooth were not tested here. No claim is made for
  them.

## Requests sent

Only the two read commands above, the same pair the reference Linux reader behind this note
sends every minute. No settings, pairing, lighting, identity or firmware command is sent, so
there is no path from this driver to the receiver's update mode.

## Tests

`tests/receiver_frames_test.cpp` decodes the captured reply above without any hardware and
recomputes its checksum:

```
g++ -std=c++17 -O2 -Wall -Wextra tests/receiver_frames_test.cpp -o build/receiver_frames_test
./build/receiver_frames_test
```

Related: the same device family appears in [Device request #1288](https://github.com/krisgfx9/Wireless-Device-Battery-Monitor/issues/1288),
which lists `260D:1154` rather than the `3554:F527` id measured here.

# Audeze Maxwell

## Scope and hardware validation

`src/drivers/audeze.cpp` reads Audeze Maxwell headsets (vendor `3329`) through the
vendor collection `FF13:0001` (62-byte input and output reports) of these
product IDs:

| Product ID | Connection | Hardware |
|---|---|---|
| `4B19` | Maxwell dongle (PC / PlayStation) | tested in HaloBattery |
| `4B1A` | Maxwell USB-C cable | tested in HaloBattery |
| `4B18` | Maxwell for Xbox dongle | tested here, see below |
| `4B1E` | Maxwell for Xbox USB-C cable | tested here, see below |
| `4B29`, `4B28` | Maxwell 2 dongle, Maxwell 2 for Xbox dongle | untested |

Cable IDs are read first: a headset on its cable and on the dongle at once is
one device, shown once, as charging.

The Xbox dongle was tested on Windows 11 (build 26300) with an Audeze Maxwell
for Xbox Series X|S, headset on and playing audio. Windows lists the USB device
as `Audeze Maxwell XBOX Dongle`; with the headset linked, its three HID
collections (`000C:0001`, `FF13:0001` and `000B:0005`, all on interface 0)
report the product name `Audeze Maxwell HID`.

At first the dongle was stuck (see [Stuck dongle](#stuck-dongle)): every read
for 1.5 s after a battery query, sent by `WriteFile` or by
`HidD_SetOutputReport`, returned `07 00 80` followed by zeros, and nothing came
on the interrupt pipe. The app showed the headset as asleep after one poll and
with the replug advice from the second. After the dongle was unplugged for about
5 seconds and plugged back in, the first read, 60–80 ms after the query, held
the answer:

```
07 10 80 05 5B 03 00 D6 0C 00 05 5D 05 00 D6 0C 00 00 44 01 4A AE 2B D3 44 C7
80 01 05 5C 03 00 80 2C 03 05 5B 03 00 D6 0C 00 05 5D 05 00 D6 0C 00 00 45 ...
```

Length `10`, so 16 new bytes: the acknowledgement and the result, `44` = 68%. The rest is
left over from earlier replies and holds an older result, `45` = 69%. The
following reads repeated the frame with length `00`. `--list` printed:

```
Audeze Maxwell  [2.4 GHz dongle]  68%   (audeze:maxwell)
```

The same headset was then connected by its USB-C cable. On its own, the cable
enumerates as `3329:4B1E`, the ID the driver had derived from `4B18`: a
composite USB device named `Audeze Maxwell XBOX Headset`, with two audio
interfaces and HID interface 0, whose three collections report the product
name `USB-Audeze Maxwell HID`. It answers the same query the same way, except
that byte 2 of the reply is `00` instead of the dongle's `80`:

```
07 10 00 05 5B 03 00 D6 0C 00 05 5D 05 00 D6 0C 00 00 45 ...
```

`--list` printed `Audeze Maxwell  [USB cable]  69%, charging`, and the
headset's LED showed it charging.

With the dongle plugged in as well, the cable re-enumerated as a HID-only
device: no audio interfaces and no interface number, and its collections are
named `Audeze Maxwell XBOX Headset`. The dongle kept the audio and reported
`Audeze Maxwell HID`. Both answered the battery query (cable 75%, dongle 76% a
minute later, while charging), and the app showed the headset once, read over
the cable:

```
Audeze Maxwell  [USB cable]  75%, charging   (audeze:maxwell)
```

With the headset then switched off, cable and dongle still plugged in, the
dongle re-enumerated without its audio interfaces and named itself
`Audeze Maxwell XBOX Dongle`. It answered the query only with frames of length
0 that still held its last result (76%), so the driver leaves it alone. The
cable kept answering while the headset was off and charging, after three `5C`
notifications:

```
07 25 00 05 5C 03 00 80 2C 03 05 5C 03 00 80 2C 01 05 5C 03 00 80 2C 00
05 5B 03 00 D6 0C 00 05 5D 05 00 D6 0C 00 00 4D ...
```

`--list` printed `Audeze Maxwell  [USB cable]  77%, charging`: a switched-off
headset on its cable is shown with the level it is charging at, as HaloBattery
measured on the PC model.

## Protocol

Sources: HaloBattery (`providers/audeze.py`, MIT) and HeadsetControl
(`lib/devices/audeze_maxwell_device.hpp`).

The request is the single battery query, a zero-padded 62-byte output report:

```
06 07 80 05 5A 03 00 D6 0C
```

| Byte | Value |
|---|---|
| 0 | `06`, output report ID |
| 1 | `07`, length of the message that follows |
| 2 | `80`, route the message to the headset |
| 3–8 | `05 5A 03 00 D6 0C`: request (`5A`), 3-byte body, battery attribute `0CD6` |

The answer is read with `HidD_GetInputReport` on input report `07`. Byte 1 is
the length of the new data in it; `0` means "nothing new yet", whatever the rest
of the frame holds. Byte 2 is `80` through the dongle and `00` on the cable; the
driver does not check it. The headset answers after about 60–120 ms with two messages,
which can share one frame or come in separate ones (seen on the Xbox dongle: the
acknowledgement alone, then its result at the start of the next answer):

```
05 5B 03 00 D6 0C 00          acknowledgement, status 00
05 5D 05 00 D6 0C 00 00 <pct>  result: the level in percent
```

The frame keeps leftovers of earlier replies after the new data, so the driver
scans the whole frame for `D6 0C 00 00 <pct>` with `<pct>` at most 100, as
HaloBattery does, and takes the first one: new data starts at byte 3, ahead of
the leftovers. An acknowledgement followed by zero padding has the same bytes
with a 0 in the level's place, so a marker right after `5B 03 00` is skipped.

The driver reads three frames 60 ms apart and keeps the newest level. While no
level has come it keeps reading, up to 10 frames (about 0.6 s per poll). It
sends nothing else: no initialisation sequence, no settings.

There is no charging flag. A Maxwell always charges on its USB-C cable, so a
level read over the cable is shown as charging.

## Headset switched off

With no headset linked, the dongle renames itself: `Audeze Maxwell Dongle` (PC)
or `Audeze Maxwell XBOX Dongle` (Xbox), instead of `Audeze Maxwell HID`; the
Xbox dongle also drops its audio interfaces (seen above). It then still answers
with the last level it holds, so the driver does not ask it, and the headset
leaves the tray like any switched-off device. On its cable a switched-off
headset keeps answering, and is shown charging.

Measured on the Xbox dongle alone, with the headset switched off: it
enumerated as a HID-only `Audeze Maxwell XBOX Dongle`, answered the battery
query only with frames of length 0 that still held the last level (76%), and
`--list` printed `No devices with battery info found.` Switched back on, the
headset linked within a second, the dongle named itself `Audeze Maxwell HID`
with its audio again, and the first query was answered after 79 ms. Every
`--list` over the next two minutes matched the raw answers: 77%, then 76% to
74% as the level settled after charging.

Right after the headset is switched on, its first reading can be 0%
(measured on the Xbox dongle in HaloBattery). A 0% in the first 90 seconds is
shown as asleep; after that it is believed. It was not seen here: none of the
25 answers in the two minutes above was 0%.

## Stuck dongle

A dongle can stop passing requests to the headset while audio keeps playing.
It then answers every request with an empty frame:

```
07 00 80 00 00 00 ... (62 bytes)
```

Nothing sent to the dongle is known to bring it back. Unplugging it for a few
seconds and plugging it back in does. It has been reported on the Xbox dongle
`4B18` (HaloBattery PR #107), on a Maxwell 2 dongle (HeadsetControl issue #573,
which also shows that a routing-byte sweep and longer waits do not help), and
probably in HeadsetControl issue #460.

The driver counts polls in a row in which the dongle sent only empty frames. A
single one is shown as asleep, since a headset that is just waking up looks the
same. From the second one on, the headset is shown without a level and with the
advice to replug the dongle:

```
Audeze Maxwell: unknown, unplug the dongle and plug it back in
```

The tooltip and the device menu show this text, after the last known level when
there is one (`60% (last known), unplug the dongle ...`). `--list` polls only
once, so it shows the headset as asleep. Any answer resets the count. The cable
is never called stuck.

## Tests

`make test` includes `tests/audeze_driver_test.cpp`, which links the production
driver against a simulated HID transport. It checks the exact request, the
captured Xbox frames above (the new result wins over the leftover one; the
cable in both its composite and HID-only layouts, and switched off with the
renamed dongle beside it), levels
that arrive late or split across frames, an acknowledgement on its own, the
stuck dongle (asleep once, then the advice, cleared by an answer or an unplug),
the renamed dongle with the headset off, failed reads and writes, the power-on
0%, the PC dongle, the cable taking precedence, strict matching and the
one-second bound per poll.

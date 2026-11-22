# HyperX Pulsefire Dart battery protocol

Read by `PulsefireDart` in `src/drivers/hyperx.cpp`. The protocol comes from community reverse engineering of
NGENUITY:

- [santeri3700/hyperx_pulsefire_dart_reverse_engineering](https://github.com/santeri3700/hyperx_pulsefire_dart_reverse_engineering) (CC0), `protocol/index.md`
- [radoslavchobanov/PlasmaNGenuity](https://github.com/radoslavchobanov/PlasmaNGenuity) (MIT), an implementation based on those notes, `plasmangenuity/drivers/hyperx/protocol.py`

Read on hardware (Windows, 2026-11-20 and 2026-11-21): a `0951:16E1` dongle answered the heartbeat with 74%, and
later with 69% while NGENUITY showed 69%. The charging flag has not yet been observed.

## Device

| Product | USB ID | Interface | Usage page : usage | Input / output / feature (Windows) |
|---|---|---|---|---|
| Pulsefire Dart, 2.4 GHz dongle | `0951:16E1` | 2 | `FF00:0001` | 65 / 65 / 65 bytes |
| Pulsefire Dart, USB cable | `0951:16E2` | | `FF13` (not handled) | |

The 65-byte Windows lengths are the 64-byte reports plus the report ID byte. The dongle also exposes the standard
keyboard (interface 1), mouse (interface 0) and consumer-control collections. The driver only opens the `FF00`
collection.

## Query

| Request | Reply | Meaning |
|---|---|---|
| `51` (heartbeat) | `51 00 00 03 <percent> <charging> 0F ..` | `[4]` battery 0..100, `[5]` 1 = charging |
| `51` (heartbeat) | `51 00 02 EC 05 00 ..` | no level (see below) |

The driver writes report ID `00` followed by `51`, so the mouse receives `51 00 00 ...`. The reply is read
with a one-second limit and unrelated reports are skipped. Bytes `[6..10]` of the reply are undocumented
(the notes guess that `[7..8]` might be a voltage), so no voltage is reported.

The second reply is not in the sources. A real `16E1` dongle (2026-11-21) sent it in stretches of half a minute
to several minutes while NGENUITY showed 69%, with the documented 69% reply in between; what triggers it (sleep,
link state or something else) is not known yet. Its `05` is not a level: reading `[4]` regardless showed 5%. So only the `00 00 03` header gives a
level; any other heartbeat reply lists the mouse as asleep with an unknown level, and the tray keeps the last
reading (dimmed unless the user turned dimming off).

The documented reply carries no connection or sleep state (the notes list a separate connectivity report `46`
and a sleep event `FF`, with no documented request). Without any reply nothing is listed.

## Still to check on hardware

- That `[5]` is 1 while the mouse is on its charger.
- What exactly makes the dongle send `51 00 02 EC 05` (sleep, link loss, or something else).

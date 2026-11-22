# VGN devices

VGN (VGN Lab, `vgnlab.com.cn`) sells mice and keyboards under the VGN, VXE, Dragonfly and Neon
names. Their HUB client is an Electron app; its device code is plain JavaScript in the renderer,
and the vendor serves the same code as a web client at `drive.vgnlab.com.cn`. Everything below was
read out of that client (VGN VHUB 4.0.11) — the product ids, the HID collections and the report
ids are exported verbatim from its `deviceList`, and each request is a `Get…` command from the
class that handles that family.

**Read-only.** Only battery queries are implemented: no mode, profile, DPI, macro or lighting
command is ever sent.

## Connection

All VGN talking is plain HID — the installer contains no driver (`.sys`/`.inf`), only an
`elevate.exe`. **The level is only reported over the 2.4 GHz link**: on a USB cable these models
answer nothing, so the driver reports nothing for a cable. The table still lists the cable ids so
the product is recognised and claimed rather than half-reported by a generic fallback; Bluetooth
is not supported by the vendor client at all.

Three details cost most of the debugging time:

1. **The report id is not where you expect.** The client uses WebHID, whose reports do *not*
   include the report id; `hidapi` (and this app's `HidDev`) keep it. A one-byte shift turns every
   level into 0, which is exactly what an early version of this driver did.
2. **One vendor id, five to seven collections.** Only the vendor page (`FFxx`) answers; the
   keyboard/consumer collections either time out or fail the read outright. The table pins
   `usagePage`/`usage`/report id per product.
3. **A sleeping keyboard answers `0xFF`.** A 2.4 GHz VGN keyboard parks itself after a minute or
   two and replies to *every* command with `0xFF` where a length belongs. That is not 0% and it is
   not a broken device — the driver reports nothing and keeps the previous value for 30 minutes
   (`graceSec`).

## Precision

Every keyboard and mouse family reports a plain **0..100 integer percentage**; single digits are
real, not a rounding artifact. A live S99 read 79 / 78 / 74 / 73 over one discharge cycle, and a
Dragonfly / Y2 mouse reports e.g. 87 or 41. Nothing in the client multiplies or divides these:

* Search every renderer bundle for a battery value near `* 10` or `/ 10` — the only hit is
  `LxServe`, which is the **headset** path (`customDeviceController: "deviceEarphone"`, VGN Siren V2
  and VGN ZHOU). Its firmware sends 0..9 steps and the client renders `(byte + 1) * 10`. No mouse or
  keyboard is routed through it, and the driver does not claim those device ids.
* The client *can* look coarse on `Da` mice, but only in the UI: when the reply carries a voltage,
  `setDisplayLevel()` throws the reported level away and re-derives it from a 21-point voltage
  table, interpolating 5 steps per segment. The result is *uneven*, not coarse: the low end needs
  **74 mV per 1%** (3050–3420 mV only ever shows 1%–5%), while 3920–4080 mV steps roughly every
  4 mV. Two exact voltages are then bumped by `if (level == 0 || level == 15) level++`, and one is
  plainly wrong: the `>` / `<` pair does not cover `voltage === 4110`, so a full battery reads **0%**
  at exactly that millivolt. Anything at or below 15% is also pinned to a flat 15. That is why a
  widget watching the vendor app drifts from the real value; the protocol byte itself is a uniform
  1%.
* `Rongyuan` (Neon75 / Neon68 keyboards) hides a real 0: a 0 reply is mapped to 100, because the
  client reads 0 as "not ready yet" (`result[1] || 100`). `vgn-ry-mouse` does *not* do this — it
  takes `result[1]` (feature 138) or `result[2]` (dongle) verbatim. This driver keeps the 0 and
  additionally never publishes a level the device did not actually answer — see the sleeping
  handling above.

Offsets worth double-checking, because they are *not* shared between a keyboard and a mouse on the
same dongle: after the `0xF7` fetch, the keyboard (`RyServe`, `uc`) reads `result[1]` while the
mouse (`RyMsServe`) reads `result[2]`. Mixing them shifts the level by one byte.

## Families

| Family | Devices | Battery query | Level at |
|---|---|---|---|
| `Da` | F1 SE/MOBA, Y2 Ultra/Master/Pro Max, F2 Master (38 ids) | output report 8, `04` (BatteryLevel) | +5, plus voltage at +7..8 |
| `King` | Dragonfly King (4 ids) | output report 32, `GetUserMouseConfig` | +16 |
| `Kc2` | Dragonfly 3 Turbo/Turbo+, F1 V5 Turbo(+), 3 MINI Turbo (18 ids) | output report 179, `06` | `127 & +20` |
| `LdMs` | Falcon3 Extreme(+), F1 V2 Extreme (6 ids) | output report 160, `02 25` | +3 |
| `Yongjiaxin` | Dragonfly 3 SE/Pro, F1 V2 Pro/SE, Y2 V2 Pro (13 ids) | output report 0, `55 30` | +9 |
| `Tjx` | N87 (2 ids) | output report 240, `30 02` | +8 |
| `Weisheng` | S99, V98Pro/V2, S87 (4 ids) | output report 4, `00 00 1A 06` | +8 |
| `Beiying` | V98Pro V3/V4, V87 V2, N75 V2, V108, VXE V87pro (14 ids) | feature 520 B or output 19 B | +8 / +4 |
| `Rongyuan` | Neon75, Neon75 Air/He/Turbo/Extreme, Neon68 Extreme (11 ids) | feature 64 B, `130` | +1 |

All offsets are relative to the reply **after** the report id is stripped.

## Not implemented

* The Sparklink families (`flash75`, `flashextreme`, `bykeyboard`: Flash75, Neon75 Extreme,
  V98Pro V4, V87Pro V2, V108Pro). Their client code is a bundled vendor SDK that wraps the same
  reports in a "service" layer; the battery call is `getBatteryStatus()` with the frame layout
  inside the bundle. Adding them means reverse-engineering that SDK — worth a separate PR.
* The bytech family (`vgn-byMs`: F1 V2 SE+/Pro+, M3 SE+, Dragonfly 3 Plus) uses a 4-byte header
  frame (`66`, seq, len, total) and `command 130, sub 15`. The frame is documented in the notes at
  `docs/13_第三方集成指南.md` in the reverse-engineering repo; the reply parser was not finished
  under time, so those 8 ids stay unclaimed rather than guessing.
* The `vgn-ry-mouse` family (Dragonfly 3 Ultra, F1 V5 Ultra, F2 V2 Ultra, Pro Max variants) answers
  `GetPowerSupply` (138) over a *feature* report, which needs `SetFeature` first. It is the next
  candidate; the ids are in the reverse-engineering repo's table.

## Testing

`tests/vgn_protocol_test.cpp` checks every decoder against frames reconstructed from the client
(request layouts, offsets, checksums, and the "refused" cases). `tests/vgn_driver_test.cpp` runs
the real driver against a simulated HID transport: collection selection, the retry after a silent
first request, the sleeping-keyboard rejection and the "never claim what did not answer" rule.

```
make test
```

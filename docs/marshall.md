# Marshall EMBERTON II battery support

## Why the speaker was not listed

Paired with Windows, the EMBERTON II only offers audio profiles: A2DP (stereo audio) and AVRCP
(media keys). It has no Hands-Free profile, because it has no microphone. Windows reads a Classic
device's battery over Hands-Free, so it has no level for the speaker, Settings shows none, and the
Bluetooth fallback skips it.

## Where the level is

Next to its Classic audio link, the speaker has a Bluetooth LE side that advertises as
`EMBERTON II [LE]`. Marshall's app reads the battery there. It is the standard Battery Service:

| Step | Detail |
|---|---|
| Service | Battery Service `0x180F` |
| Characteristic | Battery Level `0x2A19` (read, notify) |
| Value | one byte, percent 0-100, in 10% steps |

Checked against the Marshall Bluetooth app in an Android HCI log: the app showed 50% while the
speaker sent `0x32`, then 40% while it sent `0x28`. nRF Connect lists the same service and value.

Quirk: right after an LE connection the speaker can report `0x64` (100%) for up to about two seconds
before the real level. The driver reads twice, 2.5 s apart, and keeps the second value.

## Reading it from Windows

- **Pairing the LE side fails.** Windows' `DevicePairingResult` is `Failed`, even with the speaker
  in pairing mode, so Windows never fills in its own battery property.
- **An unpaired read works.** `BluetoothLEDevice` → `GetGattServicesForUuidAsync(0x180F, Uncached)` →
  Battery Level → `ReadValueAsync(Uncached)` returns the level without pairing.
- **The LE address rotates.** The LE side uses a private random address that changes often, so the
  advertisement cannot tell this PC's speaker from another EMBERTON II nearby.

The speaker's **Google Fast Pair Message Stream** gives the address. It is an RFCOMM service
(`df21fe2c-2515-4fdb-8886-f12c4d67927c`) that is reachable through the normal Classic pairing. As
soon as it opens, the speaker sends two messages without being asked:

```text
03 01 00 03 15 ae 46               model id 15AE46 (EMBERTON II)
03 02 00 06 74 c4 92 cd b2 7c      current LE address 74:C4:92:CD:B2:7C
```

In the Android log, the phone opened an LE connection to exactly the address the stream had just
reported. The message layout is from the Fast Pair specification (Message Stream, device
information events). The speaker sends no battery message on the stream.

## What the driver does

`src/drivers/marshall.cpp`, at most every two minutes, on its own thread:

1. Finds connected Classic devices named `EMBERTON...` that expose the Message Stream service.
2. Opens the stream and listens for up to 2 s. **Nothing is sent.** It requires model id `15AE46`.
3. Starts the shared passive advertisement watcher (`ble_adverts.cpp`). Windows only opens a random
   address it has recently heard advertising; a new address is usually heard within about a second.
4. Opens the LE device at that address without pairing and reads Battery Level twice. Nothing is
   written, and notifications are not enabled.
5. Closes the LE device. The poll reports the cached level, as Bluetooth, for up to 6 minutes.

The first read takes about 7-8 s. The first poll waits at most 4 s (the project limit), so a one-shot
`BatteryHub.exe --list` usually shows the speaker only from the second run. The tray app shows it on
its next refresh.

## Limits

- The Battery Service has no charging flag, so charging is not shown.
- Only the EMBERTON II (model id `15AE46`) is matched. Other Marshall speakers with Fast Pair may work
  the same way. Add their model id to `kModelIds` once one has been checked.
- If the speaker is not connected to this PC for audio, it is not listed.

Tested on an EMBERTON II, firmware 14.0.35, with Windows 11 (build 26300).

# Keychron Q6 Ultra 8K: battery over the Ultra-Link 8K receiver

The Ultra 8K keyboards run Keychron's ZMK-based firmware, not QMK, so the K5 Max patch in
[README.md](README.md) does not apply to them. Their stock firmware only reports the battery over Bluetooth:
on the 2.4 GHz link and on the cable there is no battery command at all. This page describes a 76-byte change
to Keychron's official firmware that makes the keyboard answer the battery query Keychron's own Nape Pro
trackball answers (`A7 31` on the raw HID interface, reply `A7 31 <percent> <charging>`). BatteryHub then
shows the level and charging state over the receiver and over the USB cable.

**Only for the Q6 Ultra 8K ANSI** (USB `3434:1260`, model name `KCZKQ68K`) on official firmware **v1.0.4**
(build `Feb 10 2026`). The patch is tied to that exact image. Other Ultra 8K boards, the ISO / JIS layouts and
other firmware versions need their own addresses and are not covered.

Tested on one Q6 Ultra 8K ANSI with an Ultra-Link 8K receiver (`3434:D028`, firmware 0.1.6).

## Why a patched image and not a source build

Keychron publishes the firmware source ([Keychron/zmk](https://github.com/Keychron/zmk), branch `rtl8762g`),
and the change is small there: one more case in `kc_user_cmd` (`app/src/launcher/launcher.c`). But a build
from that source links a different prebuilt 2.4 GHz radio library than the official image contains, so it
was not flashed. The patch below changes the official image instead and leaves boot, USB, Bluetooth, the
radio stack and the firmware-update code byte-for-byte stock.

## What the patch changes

- 4 bytes in the Launcher command handler `kc_user_cmd` (`0x0404A73E`): a jump to the new code.
- 40 bytes in place of `single_tone_test_start` (`0x04050DD0`), a radio carrier test only reachable through
  a factory test command. It becomes a no-op, followed by the new handler: for sub-command `0x31` it copies
  `last_state_of_charge` and `bat_charge_state` into the reply; every other sub-command runs the original code.
- 32 bytes: the SHA-256 in the image header.

[`q6_ultra_8k_battery.S`](q6_ultra_8k_battery.S) is the assembly. [`q6_ultra_8k_patch.py`](q6_ultra_8k_patch.py)
holds the resulting bytes and applies them. No Keychron firmware is included here.

## Steps

Needs Windows and Python 3 (no packages).

1. Update the keyboard to official v1.0.4 with [Keychron Launcher](https://launcher.keychron.com) if it is on
   an older version.
2. Get the official v1.0.4 image, the file Launcher installs:
   `https://sysmgr.keychron.cn/api/upload/bin/38/1775717346220.bin`
   (305,892 bytes, SHA-256 `d32077e67316f756e41ab97279aaa66b9eb05fd5ca27df2422a6df56fc081e15`).
   **Keep this file: it is the way back.**
3. Patch it. The script refuses any other input and only writes the image that was tested
   (SHA-256 `f0e6829167deee3563b3463ea0a1420b26a4ba8d3d91b83a510ea507214b0c7d`):

   ```
   python q6_ultra_8k_patch.py official.bin battery.bin
   ```

4. Close Keychron Launcher and BatteryHub. Connect the keyboard with its **USB cable** and set the mode
   switch to **Cable**.
5. Flash. The first two commands only read; the second is a dry run that identifies the keyboard, validates
   the image and stops. The third writes.

   ```
   python q6_ultra_8k_flash.py info
   python q6_ultra_8k_flash.py flash battery.bin
   python q6_ultra_8k_flash.py flash battery.bin --yes-flash-my-keyboard
   ```

   The transfer takes about 20 seconds and goes to a spare flash area. The keyboard checks CRC32, SHA-256
   and the model name itself and is only then asked to switch, so an interrupted transfer leaves the running
   firmware in place. Do not unplug the cable while the keyboard restarts.
6. Switch back to **2.4 GHz** and check with `BatteryHub.exe --list`:
   `Keychron Q6 Ultra 8K  [2.4 GHz receiver]  38%`.

The level is not shown while the keyboard sleeps (the receiver reports the slot as not connected); the app
keeps the last value for a while.

## Going back to stock

```
python q6_ultra_8k_flash.py flash official.bin --yes-flash-my-keyboard
```

Any official update installed by Keychron Launcher also replaces the patched firmware and removes battery
reporting.

## Risk

This is modified firmware, at your own risk. It is not made or supported by Keychron.

- The patched image was flashed and used on one keyboard. Flashing the official image back with this tool
  uses the same code path but has not been done yet.
- The flashing tool only accepts the two images above, only for a keyboard that identifies as `KCZKQ68K` on
  firmware 1.0.4.
- If a firmware image ever failed to start, there is no documented recovery short of opening the keyboard.
  The patch does not touch anything that runs at boot or the update code.

## How the flashing tool works

[`q6_ultra_8k_flash.py`](q6_ultra_8k_flash.py) speaks the update protocol from Keychron's firmware source
(`app/src/dfu/tdfu.c`), the same frames Keychron Launcher sends: HID usage page `0x8C`, output report `B2`,
input report `B1`. It talks to the keyboard through the Windows HID API only; it has no network access and
starts no other program.

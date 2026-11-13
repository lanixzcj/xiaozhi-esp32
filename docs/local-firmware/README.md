# Local recovery and gentle games

This branch integrates the safeguards from the physically accepted 2026-10-18
firmware with `cursor/dot-tap-toddler-game-9b1a` at `4e9b7cd59e5f42e61a60f71e4f58c600929ce08e`.
It includes animal, transport and dot games, quieter replacement cat/dog recordings,
and game entries positioned below the real status bar, updated after theme changes.

Historical source patches and the accepted build manifest are preserved in
[2026-10-18](2026-10-18/README.md). Credentials and device backups are local only.

Build with official ESP-IDF 6.1 after sourcing its environment:

```sh
bash scripts/build_gentle_games.sh
```

The committed build profile selects the original lichuang-dev ESP32-S3 N16R8,
zh-CN, LVGL, device AEC and 16 MiB partition table. Network conversation and
activation remain enabled. Remote firmware upgrades and shared asset downloads
are blocked; NVS initialization errors stop instead of erasing settings. Automatic
OTA validity confirmation is disabled so physical acceptance is explicit.

Only the app is deployed to the inactive OTA slot after an independent full backup
and partition/identity check. No bootloader, partition, initial OTA data, NVS or
shared assets are included in the device write. Never use the generic full-flash
commands printed by ESP-IDF for this deployment.

A successful build and host UI test do not establish physical touch, audio,
network, tilt or power-on operation. Physical acceptance and restart retention
must be recorded separately, after checking the running app hash.

## R2 interaction corrections

The menu is now a right-side vertical column (animal, transport, dots). Correct
answers pulse only the card outline and use a shorter minimum feedback interval;
photos are no longer transformed into an extra scaled layer.

The QMI8658 normal measurement values are CTRL2=0x15 and CTRL3=0x55.
Earlier values 0x95/0xD5 erroneously enabled bit7 self-test. See the manufacturer's
[register and self-test specification](https://files.waveshare.com/wiki/common/QMI8658C_datasheet_rev_0.9.pdf),
pages 30–31 and 55. Configuration is read back, and failure disables the sensor
instead of feeding invalid motion. This is a verified configuration defect;
its physical contribution to the reported direction bias still needs device testing.

Dots can be dragged in every direction; dragging is not a tap or long-hold shake.
The mixed/custom page also advances after three taps, and a 700 ms hold always
works, even with a working IMU. The original shake interaction remains available.
The tap-only sequence is one yellow dot → five yellow dots → mixed colors →
three more taps → scatter → cross → tap to restart.

## R3 sensitivity and clarity

User feedback on R2 still found tilt sluggish and cat/dog unclear on the speaker.
R3 learns the initial holding pose from four valid acceleration samples, reduces
the motion deadzone and spring pull, increases gravity response, and polls at
33 ms. A ~14 degree relative tilt can move a central dot near an edge in two
seconds in the host regression. Actual mounting and feel need device testing.
Cat/dog adapted WAVs receive +8 dB gain, keeping filtering/fades and preserving
all other cues and the global volume setting. Their decoded peaks are ~-10 dBFS.

## R4 upright tilt correction

The user holds the screen facing them, nearly upright. R3 subtracts only X/Y
acceleration; around upright, forward and backward pitch both reduce Y, so the
same command is produced for opposite tilts. R4 normalizes complete X/Y/Z gravity
vectors and projects their cross product onto two tangent control axes at the
entry pose. Z now preserves the missing direction; the right-control axis adapts
between flat and upright holding. There is no accumulated gyro integration drift.
Response is 1.4 times sine of relative tilt, with the existing damping/deadzone.
Strong linear acceleration and free fall produce no tilt command.

Host regressions use physically consistent gravity at 0/30/60/85/90 degree poses,
require all four screen edges and reversals, and exercise upright samples through
the actual LVGL game timers. These checks do not prove the physical mounting or
feel; user confirmation remains required before OTA validity is committed.
Cat/dog changes are deferred at the user's request; R3 audio remains as is.

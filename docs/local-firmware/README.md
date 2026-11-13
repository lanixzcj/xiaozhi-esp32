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

## R5 two-piece photo puzzle and lighter dot transitions

For a child under two, the new fourth right-side entry opens a two-piece animal
photo puzzle. Large 148×162 pixel halves reuse the existing licensed 296×162
photos; child image clipping requires no transformed bitmap allocation. Pieces
can be dragged, get a generous 52×40 pixel snap tolerance, and remain movable
after an incomplete attempt. A fitted piece is placed behind the remaining one
so it cannot hide the last draggable half. Completion waits for a parent's tap
on 再玩; there are no scores, timers, automatic advances or tilt controls.
The duck, cow, sheep and chicken photos rotate. BOOT and 返回 exit the puzzle;
conversation and push-to-talk handlers observe the same local-interaction guard.
No audio assets or global volume are changed.

Dot scatter/return retain their paths and timings but do not scale five circles
simultaneously. Host UI checks require normal scale during scatter. R4 tilt
mapping and dynamics are unchanged. Physical smoothness still requires testing.

## R6 dot game name and persistent touch return

The home label is 点点点. A touch 返回 button is now a direct child of the game
panel, visible on all pages rather than only the rest overlay. It stays above
the rest overlay, and its hold cannot trigger the mixed-page shake fallback.
The two top yellow dots and the upper-right mixed dot are moved below the return
button. BOOT still exits. Actual LVGL pointer tests cover return from all built-in
pages, including scatter, settling and rest. Tilt, puzzle and audio are unchanged.

## R7 clear illustration puzzle

User feedback found the real photos unclear as a left/right puzzle. Puzzle assets
are now original, flat apple/bus/fish illustrations, each with a large subject,
plain background and thick outline across the center split. A complete thumbnail
in the header shows the target. After both halves snap, internal outlines vanish
and the complete image stays visible until manual next. Other games retain their
existing photos, sounds, tilt behavior and touch return. Three opaque I4 images
and their native-size previews need 77,592 pixel/palette bytes in flash; they are generated offline from vector
primitives. Source previews and licensing are in assets/puzzle/README.md.

## R8 full-screen seconds clock

A fifth right-column 时钟 entry opens a full-screen 52px HH:MM:SS clock with date
and touch/BOOT return. Entry spacing is 40px, retaining 72x38 buttons; all five
fit a 320x240 screen below the status bar. The clock reads the existing device
system time every 200ms and only changes text when the wall-clock second changes.
It does not increment a counter, alter TZ, start a voice turn or block ongoing
conversation. The OTA response already applies its timezone offset before
settimeofday; adding another UTC+8 offset would be wrong. Network time corrections
are reflected immediately; unsynchronized startup shows --:--:-- and 等待校时.
After synchronization it continues ticking offline until reset; network time is
needed again when the device has no valid clock. Games and model behavior stay
unchanged. Host tests cover seconds, midnight/date rollover, wall-time correction,
unsynchronized state, touch exit, heap stability and five-button bounds.

R7 was explicitly accepted by the user, marked VALID after app/metadata readback,
and its R7 version was observed after a serial-triggered reset. It remains locally
restorable before any R8 trial write; the original stable OTA1 is also preserved.

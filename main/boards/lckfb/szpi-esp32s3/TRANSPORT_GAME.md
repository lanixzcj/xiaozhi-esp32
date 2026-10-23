# Transport matching development

This branch is source development only, not a hardware-accepted replacement firmware.
The baseline is `f7b1c8c6d6289fcc2adc5416999b2dd7b1ce2b3f` from
`feat/lichuang-animal-touch-game`. The later accepted animal-game firmware/source
was not present on the remote at implementation time (2026-10-23).
Its exact two-choice animal interaction cannot be claimed recovered here.
Existing remote animal assets and session behavior remain intact. Its entry has
been moved beside the new transport entry in the top-right area; game ownership
is exclusive. These are reconstructed changes, not a recovered final baseline.

## Interaction

- Tap the top-right transport entry. This reuses the existing local-interaction
  audio ownership, suspending remote conversation without deleting the chat UI.
- Tap listen, then choose between two large vehicle pictures. Each question has
  exactly one matching option. Answers alternate sides across rounds/sessions.
- Wrong answers invite another attempt without punishment or automatic noise.
  Replay is explicit, never queued during playback. Correct answers advance after
  short visual feedback. No next-question sound starts automatically.
- Four completed questions or a 90-second session limit shows a rest screen.
  A parent holds restart for 1.5 seconds and releases to start another session.
- Return or BOOT exits, cancels local playback and uses existing audio draining /
  conversation-restoration logic. The other game's overlay cannot acquire the
  same audio session. Provisioning/upgrade lifecycle cancellation also exits.
- No network fetch, NVS writes, bootloader, partitions, pins, security settings,
  automatic OTA, or backup uploads are added or modified.

## Verification

Run `python3 -m unittest discover -s scripts/tests -v` for host regression tests.
The matching policy tests cover both answer sides, distinct options, wrong-answer
retry, replay, no advance while sound plays, rest, session timeout and tick wrap.
The existing animal and local-interaction tests remain applicable.
Use ESP-IDF 6.1 and the canonical build:
`python scripts/build.py lckfb/szpi-esp32s3 --name lichuang-dev`.
The dedicated development workflow runs these checks on branch pushes.

Before flashing, compare against the accepted on-device animal version and test
FT6336 release/hold behavior, both image hitboxes, BOOT during all phases,
repeated entry/exit, remote speech interruption, microphone/wake restoration,
Wi-Fi provisioning transitions, speaker volume and all four real recordings.
A host test or successful firmware build does not establish hardware acceptance.

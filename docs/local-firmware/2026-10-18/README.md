# Preserved local firmware: 2026-10-18

The physically accepted firmware was `2.5.3-animal-sound`, app SHA256
`9c7724e9de984b814a88be877a2a85031fb5dd8340a0774ab2123692af2ce114`.
It was built from `f7b1c8c6d6289fcc2adc5416999b2dd7b1ce2b3f` plus these patches, in order:

1. `find-animal-2.5.3.patch` (already includes network protection)
2. `changes-from-7a8c.patch`

`network-protected.patch` is an earlier standalone protection patch, retained for
reference only; do not apply it before the cumulative find-animal patch.
The two-patch reconstruction was verified against every file in the final build manifest.

These are historical patches, not patches to apply to the current branch.
The build manifest was captured before flashing; its `flashed: false` is historical.
Subsequent local acceptance records confirmed exact app readback, VALID selection,
animal-game and conversation operation, and battery P off/on retention.
Transport and dot games were not in that accepted app.

Current code retains the firmware/shared-assets protection and NVS error handling.
Current animal/transport listen-and-pick UI supersedes the historical animal UI.
Device backups, credentials and serial logs are local only and are never committed.

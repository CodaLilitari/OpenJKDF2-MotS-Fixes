# OpenJKDF2 Windows x64 / MotS fork changelog

This file records behavior, compatibility, build, and validation changes made
for this fork.  Upstream history remains available in Git; this log describes
the fork-specific delta and the evidence used to justify it.

## 2026-08-18

### Fixed

- Corrected the retail-default MotS inventory bindings.  F2, F3, and F4 now
  select `ACTIVATE19`, `ACTIVATE18`, and `ACTIVATE17`; Jedi Knight retains
  `ACTIVATE16`, `ACTIVATE15`, and `ACTIVATE14`.  The two mappings were verified
  directly against the official JK and MotS demo executables.  Existing player
  profiles are not rewritten; create a profile or use **Restore Defaults**.
- Corrected JkGfxMod texture-signature fallback.  A signature-only metadata
  match previously selected an empty path-cache entry instead of the matching
  signature-cache entry.  Signature identity now takes precedence over the
  OpenJKDF2 path fallback, matching JkGfxMod and distinguishing JK/MotS
  textures that share a filename.  Duplicate signature claims retain the first
  material, matching JkGfxMod's map behavior.
- Restored MotS's 32 retail inventory activation slots while preserving JK's
  20.  The shared handler and registration code had hardcoded JK's limit, and
  one translated pointer bound processed an unintended 21st JK entry.  Both
  paths now use exact indexed, mode-aware bounds; save/network widths remain
  selected by their serialized version.

### Build and tests

- Added a fork-only full MinGW Windows x64 workflow.  It verifies PE32+ AMD64
  architecture, non-system DLL closure, source/submodule provenance, hashes,
  and the compiler/toolchain record.  It does not upload the resulting binary.
- Added host-side regression tests for JK/MotS default inventory bindings and
  20/32-slot capacity, plus JkGfxMod path/signature lookup, signature
  precedence, first-claim behavior, and non-mutating cache misses.
- Added a distribution/provenance record and third-party notice inventory for
  the Windows x64 evaluation package.

### Licensed-data validation

- Preserved the Steam Jedi Knight installation and a verified US 1.0 MotS disc
  image as read-only sources, then created separate clean and mutable test
  lanes outside the repository.
- Confirmed the existing Windows x64 fork build launches both licensed games.
  Jedi Knight reached playable Nar Shaddaa first-person rendering; MotS loaded
  and rendered the opening New Republic Base sequence.  Twenty-second headless
  smoke runs for both games produced no crash dump, missing-container error, or
  assertion.  Full-campaign, multiplayer, save-compatibility, and long-soak
  testing remain open.

### Patch and mod triage

- Did not apply the Desktop unofficial-patch bundle to either clean lane.  Its
  changes are unsigned 32-bit replacement retail executables (including CD
  check changes), not source patches for OpenJKDF2's 64-bit implementation.
- Kept MotS+ and Jedi Knight Remastered assets out of the retail baseline and
  repository.  They are useful as isolated mod-compatibility test corpora, not
  as retail behavior references.

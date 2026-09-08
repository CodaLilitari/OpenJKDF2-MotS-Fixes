# OpenMotS fork changelog

This file records behavior, compatibility, build, and validation changes made
for this fork.  Upstream history remains available in Git; this log describes
the fork-specific delta and the evidence used to justify it.

## 2026-09-08

### Fork identity and attribution

- Named the bugfix fork OpenMotS, with a Windows x64 and MotS retail-fidelity
  focus while retaining JK support.
- Added credit to Andrei Kiralv/Darkahn and Lilitari (OpenAI Codex Interlocutor)
  for the original fork fixes, and invited upstream merging or cherry-picking
  with attribution. Existing upstream credits and license terms are preserved.
- Updated both the README and its generation template so regeneration retains
  the fork introduction. Labeled upstream release and issue links explicitly.
- This update changes documentation and repository identity only; the engine
  behavior and `openjkdf2-64` executable/build target are unchanged.

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
- Grouped the MinGW static runtime archives so their circular references are
  resolved by current linkers.  This restores x64 cross-build compatibility
  with MinGW-w64 13, whose `libmsvcrt.a` math wrappers can call back into
  MinGW runtime archives after a single-pass link has already scanned them.
- Added host-side regression tests for JK/MotS default inventory bindings and
  20/32-slot capacity, plus JkGfxMod path/signature lookup, signature
  precedence, first-claim behavior, and non-mutating cache misses.
- Added a distribution/provenance record and third-party notice inventory for
  the Windows x64 evaluation package.

### Licensed-data validation

- Preserved the Steam Jedi Knight installation and a verified US 1.0 MotS disc
  image as read-only sources, then created separate clean and mutable test
  lanes outside the repository.
- Built implementation commit `a857b632` in full Release configuration with
  MinGW-w64 13.  The resulting `openjkdf2-64.exe` has SHA-256
  `C9B403134D65BF27FC3398B6E30D94E4AD24CECFD63FB1AA5CD4F6671381799E`;
  an independent PE parser confirmed COFF machine `0x8664` and PE32+ magic
  `0x020B` for the executable and all three packaged runtime DLLs.
- Ran fresh twenty-second headless smoke tests against clean JK Steam and MotS
  US 1.0 retail-data copies.  Both processes remained in their main loops until
  the harness stopped their exact PIDs.  OpenAL and GameNetworkingSockets came
  from the isolated x64 runtime directories, while `WINMM.dll`, `KERNEL32.dll`,
  and `msvcrt.dll` resolved from `System32`.  There was no crash report,
  assertion, missing GOB/GOO error, fatal error, or exception; the only log
  output was the pre-existing UNI declared-line-count diagnostic.
- Re-hashed both clean baselines after the runs.  Their deterministic manifest
  digests remained
  `4DA0D81CCD14FD5352B4A94DEF0BE2425A59450F18DD4C98A2635BE99C466149`
  (JK) and
  `751111429BCAAD8524BB1D71216C4644D438A1471B46CBDCC9EB5441D4F9B0C`
  (MotS), proving that validation changed only the disposable lanes.  Earlier
  interactive validation reached playable Nar Shaddaa rendering and rendered
  MotS's opening New Republic Base sequence.  Full-campaign, multiplayer,
  save-compatibility, and long-soak testing remain open.

### Patch and mod triage

- Did not apply the Desktop unofficial-patch bundle to either clean lane.  Its
  changes are unsigned 32-bit replacement retail executables (including CD
  check changes), not source patches for OpenJKDF2's 64-bit implementation.
- Kept MotS+ and Jedi Knight Remastered assets out of the retail baseline and
  repository.  They are useful as isolated mod-compatibility test corpora, not
  as retail behavior references.

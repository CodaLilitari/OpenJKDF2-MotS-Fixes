# OpenJKDF2-MotS-Fixes retail-fidelity Windows x64 fork and evaluation build

OpenJKDF2-MotS-Fixes is the bugfix fork at `andreipemberton/OpenJKDF2-MotS-Fixes`. It tracks
`shinyquagsire23/OpenJKDF2` without granting its `upstream`
remote a push URL. Changes are validated on `retail-fidelity-x64` before the
fork's default branch is fast-forwarded. No upstream pull request is created
by this workflow. Fork-specific history is summarized in `CHANGELOG_FORK.md`.

## Baseline

- Upstream commit: `0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac`
- Validated implementation commit: `a857b6320af71c0e7a526dfc9d8441a43bac0723`
- Target: Windows x64 (`openjkdf2-64.exe`)
- Build configuration: full MinGW x86-64 Release build, without the reduced
  `GITHUB_RUNNER_COMPILE` dependency set

## MotS default inventory hotkeys

OpenJKDF2 issue #373 documents that the MotS Field Light cannot be activated
with its retail-default F2 key. The original implementation always used the
JK hotkey slots even though MotS inserts three additional hotkeyable Force
powers before the inventory items.

The fork preserves the JK defaults and selects the distinct MotS defaults:

| Key | Jedi Knight | Mysteries of the Sith |
| --- | --- | --- |
| F2 | `ACTIVATE16` (Field Light) | `ACTIVATE19` (Field Light) |
| F3 | `ACTIVATE15` (IR Goggles) | `ACTIVATE18` (IR Goggles) |
| F4 | `ACTIVATE14` (Bacta Tank) | `ACTIVATE17` (Bacta Tank) |

The values were confirmed directly against the official demo executables:

- JK `jkdemo.exe` SHA-256:
  `0F8A1B01CFDCB196B043EBB4EBF42DC3A6D4FA394034EBFB8CAD9E383E14AA46`
- MotS `jkm.exe` SHA-256:
  `A122E766F8DA4E84C11DD916E50EC35D9842A28357A3CB61600DF4A3573F281E`

The change applies when creating a profile or choosing **Restore Defaults**.
Existing profile bindings are intentionally not rewritten, so custom layouts
remain intact.

## JK/MotS inventory activation capacity

The original JK input table processes and registers 20 inventory activation
slots. MotS expands the same table to 32. The shared OpenJKDF2 implementation
hardcoded the JK limit in both live input handling and registration; a second
translated bound compared a struct pointer with the address of an `idk` field,
which accidentally processed a 21st entry in JK.

The fork uses indexed, mode-aware bounds in both paths: 20 for JK and 32 for
MotS. Save and network synchronization deliberately continue to choose 20 or
32 from the serialized protocol/save version, not the current launch mode.
The counts were confirmed in the official JK demo (`sithInventory_KeybindInit`
at VA `0x004D33D0`) and both the official MotS demo (VA `0x005212D0`) and owned
US 1.0 retail `JKM.EXE` (VA `0x00521DE0`, SHA-256
`23CD064217F904D36D896F52D3CDD17635BB85441A12E0415C0637504191D974`).

## JkGfxMod texture-signature fallback

JkGfxMod metadata may identify a replacement by material path/cel, by the MD5
signature of the original texture, or by both. The previous lookup detected a
signature hit but then unconditionally indexed the path cache. On a
signature-only match, that inserted and selected a blank path entry; the
matched signature entry was never used.

The fork now follows JkGfxMod's signature identity first and keeps OpenJKDF2's
path/cel mapping as a fallback for metadata that omits a signature. The first
material to claim a signature remains authoritative, as in JkGfxMod. Lookup
uses `find()` only, so a miss cannot mutate either cache. A synthetic host
regression test verifies path-only lookup, signature-only lookup, deliberate
signature precedence, complete misses, and repeated non-mutating misses
without requiring game or mod assets.

JkGfxMod's first-claim rule still leaves a separate pack-order ambiguity when
different materials claim the same signature. The inspected Remastered 5.11
corpus contains 652 duplicated signature values, including cross-pack JK/MotS
claims. Directory traversal does not define a semantic game-specific priority.
A future hardening change should make traversal deterministic, report
conflicting claims, and prefer the active game's material set without falling
back to globally path-first matching.

## Automated x64 verification

The fork-only `Windows x64 retail-fidelity` workflow:

1. checks out the exact source and recursive submodule commits;
2. runs the fork's asset-free host regression tests;
3. builds the full `openjkdf2-64` MinGW target;
4. packages the executable, non-system runtime DLLs, and license notices;
5. requires every packaged PE to use COFF machine `0x8664` and optional-header
   magic `0x020B`;
6. checks that every imported non-system DLL is packaged;
7. records source, submodule, import, architecture, and toolchain evidence; and
8. creates SHA-256 manifests for the files and final archive.

The generated package contains no LucasArts assets. A licensed Jedi Knight or
MotS installation is still required to run the engine.

## Distribution and provenance boundary

The workflow produces and verifies an engineering evaluation package only in
its ephemeral runner workspace; it deliberately does not upload a binary
artifact. During the audit, pre-existing upstream source comments were found
that explicitly describe some engine code as derived from OpenJones3D, whose
public repository is AGPL-3.0. No independent provenance or compatible
dual-license grant for those portions was verified in this work. This is not a
finding that the upstream project violated a license, but it is an unresolved
redistribution question. Do not publish or broadly redistribute a binary until
the relevant authors establish compatible provenance or the affected
implementations receive a documented clean replacement.

The workflow records source commits, submodule revisions, nested build-time
patches, exact compiler package versions, and hashes. This provides a strong
audit trail, but the build is not claimed to be bit-for-bit reproducible: the
Ubuntu package repository, archive metadata, and compiler environment are not
fully hermetic.

## Current validation boundary

A pristine Steam Jedi Knight installation and a verified US 1.0 MotS disc data
track were staged into isolated test lanes outside the repository. The source
media and Steam installation were not patched or modified. No LucasArts game
asset is stored in Git or sent to the build workflow.

Implementation commit `a857b632` completed a full Release build under Ubuntu
26.04 with MinGW-w64 13. The resulting `openjkdf2-64.exe` has SHA-256
`C9B403134D65BF27FC3398B6E30D94E4AD24CECFD63FB1AA5CD4F6671381799E`.
An independent Windows PE parser confirmed COFF machine `0x8664` and optional-
header magic `0x020B` for the executable, OpenAL, GameNetworkingSockets, and
FreeGLUT. Static import inspection found no unprovided non-system dependency.

Fresh disposable JK and MotS lanes each completed a twenty-second headless
smoke run: both processes remained in their main loops until the harness stopped
their exact PIDs. `OpenAL32.dll` and `libGameNetworkingSockets.dll` loaded from
the isolated x64 runtime directories; `WINMM.dll`, `KERNEL32.dll`, and
`msvcrt.dll` loaded from Windows `System32`. No crash report, missing GOB/GOO
container, assertion, fatal error, or exception was produced. The only debug-log
messages were the same pre-existing UNI declared-line-count diagnostics seen in
the earlier build. Standard output and standard error were empty.

The clean-source manifests were re-hashed after both runs and remained exactly
`4DA0D81CCD14FD5352B4A94DEF0BE2425A59450F18DD4C98A2635BE99C466149`
for the 64-file Steam JK tree and
`751111429BCAAD8524BB1D71216C4644D438A1471B46CBDCC9EB5441D4F9B0C`
for the 48-file MotS tree. Interactive validation with the earlier x64 build
reached playable first-person Nar Shaddaa rendering and rendered the opening
New Republic Base sequence in MotS. These checks establish licensed-data
startup, parser/resource compatibility, runtime dependency resolution, and
basic rendering; they do not establish full retail parity. Full-campaign,
multiplayer, save-version, audio-track, mod-matrix, graphical texture-override,
interactive default-binding, and long-soak testing remain required.

The Desktop unofficial-patch bundle was not applied. Its payload replaces the
original 32-bit `JK.EXE`/`JKM.EXE` code and removes legacy CD checks; OpenJKDF2
does not execute those binaries, so copying them into the clean data lane would
not repair the 64-bit source rebuild and would contaminate the retail oracle.

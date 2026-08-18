# Retail-fidelity Windows x64 fork

This fork tracks `shinyquagsire23/OpenJKDF2` without granting its `upstream`
remote a push URL. Development is isolated on the `retail-fidelity-x64`
branch. No upstream pull request is created by this workflow.

## Baseline

- Upstream commit: `0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac`
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

## Automated x64 verification

The fork-only `Windows x64 retail-fidelity` workflow:

1. checks out the exact source and recursive submodule commits;
2. builds the full `openjkdf2-64` MinGW target;
3. packages the executable and non-system runtime DLLs;
4. requires COFF machine `0x8664` and optional-header magic `0x020B`;
5. checks that every imported non-system DLL is packaged;
6. records source, submodule, import, architecture, and toolchain evidence; and
7. creates SHA-256 manifests for the files and final archive.

The generated package contains no LucasArts assets. A licensed Jedi Knight or
MotS installation is still required to run the engine.

## Current validation boundary

No licensed JK or MotS installation was found on the build workstation during
this change. Source-level verification, original-demo differential analysis,
and a Windows x64 build are therefore in scope now. An in-game test using a new
MotS profile or **Restore Defaults** remains required once licensed game data is
available locally.

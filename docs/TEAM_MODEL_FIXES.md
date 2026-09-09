# Multiplayer class/team model fixes

This fork's September 9, 2026 correction addresses the class/team lookup in
`src/Dss/jkDSS.c`. It changes engine code; it requires no edited models, textures,
game archives or mod archives. World Between Worlds remains a separate project.

## Findings and behavior

MotS's native character menu assigns personalities 1 through 4, whereas the
engine's team-model table contains four zero-based rows of five entries each.
The old `personality * 5 + team` expression chose the following class's row for
the first three classes and went past the table for Soldier (personality 4),
even with a valid team number. One packet handler also admitted negative
personalities. Other code paths can set a player's team independently, so the
model-selection boundary must validate both indices itself.

| Native personality | Class | Correct model-table row |
| --- | --- | --- |
| 1 | Jedi | 0 |
| 2 | Bounty Hunter | 1 |
| 3 | Scout | 2 |
| 4 | Soldier | 3 |

The shared lookup validates the inputs before doing arithmetic. It accepts
teams 0 through 4 for character model selection; the existing team-change
command still requires 1 through 4. Personality 0 retains the old first-row
fallback used by an unset/single-player personality. This preservation choice
is not a claim that 0 is a native multiplayer Jedi class.

For forced-team character updates, an unsupported class or team uses the
existing `kk.3do` / `ky.snd` fallback. A forced-team change with an unsupported
personality is rejected before changing the team, model, broadcast or scores.
Custom skins/personality values are not globally rejected: ordinary skin
selection outside forced-team substitution and the server's selected-model
broadcast retain their existing paths. JK's adjacent character handler also
rejects an invalid team before indexing its five-entry model table.

The Soldier table's last two filenames were also reversed. Team 3 now selects
the blue model (`kym13b.3do`), and team 4 the green model (`kym13g.3do`). This
correction follows the original game's documented team order and model labels.

## Source evidence

- `src/Gui/jkGUIBuildMulti.c`: character creation assigns the selected
  personality index plus one. `src/World/jkPlayer.c` orders the named classes as
  single, Jedi, Bounty Hunter, Scout and Soldier.
- Native character files produced by the previously inspected MotS-mode menu
  independently contain the multiplayer values 1, 2, 3 and 4.
- The owned original MotS `JKMRES.GOO` member `misc/models.dat` associates
  `ky`, `kyh4`, `kypr` and `kym13` with those four multiplayer classes. Its
  SHA-256 is `3882a33a9ba37afaca4d381e1aaddea20c7066841d979b83d36c9aaa62bd8be6`.
- The original MotS README's multiplayer Team command documents team 1 red,
  2 gold, 3 blue and 4 green. Native model comments and localized model names
  identify the Soldier's `b` and `g` variants as blue and green.
- `sithCogFunctionPlayer_SetPlayerTeam` and received score synchronization can
  write a team outside the guarded team-change handler. Its guard alone could
  not make the later character-model lookup safe.

Only this description and original regression code are included here. The
owned source archives, extracted resource files and generated characters stay
outside the repository.

## Regression scope

Run the asset-free native test with GCC or Clang:

```sh
python3 src/Tests/run_team_model_test.py --cc gcc --ubsan
```

The runner extracts the current production model tables, lookup helper and
complete character/team handlers. Explicit stubs observe model loads, sound,
saber initialization, broadcasts and score synchronization. These tests do not
render a skin or establish actual peer synchronization. An old-index negative
control must fail under UndefinedBehaviorSanitizer on the Soldier case.

The September 9 local GCC 15.2 / UBSan run passed 102 cases and 2,037 assertions.
Restoring the old lookup failed the independent Jedi model expectation, and
UBSan detected the Soldier index-24 overread through each MotS handler. The
existing input/cache, COG-locality, backpack, bitmap and version checks also
passed locally. The renderer and game loop are not part of this native harness.

The Windows x64 retail-fidelity workflow runs this test alongside the existing
fork regressions. A local result must not be described as a GitHub Actions run.
Full gameplay, rendered appearance and mod-wide compatibility remain separate
acceptance checks in [MOD_COMPATIBILITY.md](../MOD_COMPATIBILITY.md).

## Separate open finding: skin-list personality filtering

The MotS `models.dat` fourth column is not consumed by the current shared
character-screen parser, whose model record lacks a personality field. Native
MotS data has 55 rows with class metadata. SBX-MotS 3.0 has 676 rows: four
identical 165-row model/sound lists tagged for classes 1 through 4, plus 16
class-0 team entries. The current next/previous controls cycle the entire list.

The missing metadata/filtering is source-confirmed. Its correction still needs
an explicit behavior check for class 0, older three-column skin packs, custom
personality IDs and existing saved characters whose model is outside their
class. This patch does not impose a speculative filter or claim that all JK
skins are now fully compatible with MotS.

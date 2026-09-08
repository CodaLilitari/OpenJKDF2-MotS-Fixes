# MotS mod compatibility

Compatibility with MotS gameplay mods is a required validation target for this
fork. A successful archive load or short host run does not establish complete
gameplay, rendering, multiplayer synchronization, or crash-free compatibility.

## Engine and loading

Run the source-port executable with the owned **MotS** data:

```text
openjkdf2-64.exe -motsCompat -path mods/sbx3
openjkdf2-64.exe -motsCompat -path mods/manowars3
```

These are separate launches: select **one gameplay mod per run**. The `-path`
argument is a directory containing the mod's resource archives, not a GOO
filename. A level/episode is selected separately. GOO is the container format
for both, and does not by itself identify an archive as a mod or an episode.

For SBX, put the original `SBX-MotS v3.goo` in `mods/sbx3`. For Manowar 3, put
the author's two core resource archives, `Mano3sd.goo` and `Mano3sk.goo`, together
in `mods/manowars3`; together they form one mod. Its bonus episode archives are
optional levels, not substitutes for the core mod.

Use clean, separate data folders/profiles for comparisons. Direct `mods/*.goo`
files are automatically loaded; nested mod folders remain selectable using
`-path`. Root and other loose-file overrides can supersede archive resources.
Do not infer a universal filename-based precedence rule for conflicting files.

Historical unofficial-patch instructions for the original `JKM.EXE` concern
that retail executable. They are not prerequisites for launching this source
port. Enhancement compilations can replace gameplay scripts and assets:
[EMSITH's author instructions](https://www.moddb.com/mods/todoa/downloads/emsith)
explicitly warn against combining it with other MotS mods by default. Test such
combinations separately from the original SBX/Manowar packages.

## Audited packages

| Package | Primary archive | ZIP SHA-256 | Activation entries |
| --- | --- | --- | --- |
| ShadowX's Saber Battle X, MotS 3.0, 2002-01-07 | [Massassi](https://www.massassi.net/levels/files/1866.shtml) | `909ad10a6bbe3954e0e7c20f5d55094a5fb68708716866102d439d8aac339908` | 26 |
| Azlon's Manowar 3 v1.0, 1999-01-12 | [Archived author site](https://jkdf2.net/JKBot/JKArchive/JKDF2/JKDF2/Websites/JKMAG%20-%20The%20Jedi%20Knight%20Multiplayer%20Add-on%20Group/man3/index.html) | `75daf2f82708bc7a88082b4b0b420931845175b10c00b91d08389610e9a82184` | 31 |

Both exceed upstream's former 20-entry activation handling and fit the fork's
32-entry MotS range. Their original author credits and documented defects must
remain distinct from source-port regressions. No mod or retail assets are
included in this repository.

## September 2026 findings

- Autostart initialized player profiles before loading the static inventory
  table. Profile reads therefore rejected unregistered activation functions and
  wrote the profile back without those bindings. Initialize the inventory first.
  A paired process test preserved an ordinary control in both builds; SBX's
  function 67 and Manowar's function 72 were lost with the old order and
  preserved with the repaired order. Those initial profile-regression probes
  used SP startup and are not evidence of mod multiplayer gameplay parity.
- Fresh autostart could inherit an empty profile name previously saved by GUI
  startup. Treat an empty name as unset and use the existing `ServerDed` default.
- A headless SP mission briefing waited for acknowledgment. Skip that dialog
  in headless mode while retaining loading cleanup. SP cutscene testing is
  separate from the multiplayer mod acceptance tests.
- Headless Windows startup allocated a console and briefly created a visible
  SDL bootstrap window. Preserve inherited streams and hide the bootstrap;
  validate console handles before reading events.
- Headless POV rendering called `std3D_ClearZBuffer` without an initialized GL
  framebuffer. An embedded exception report identified the null read through
  `jkPlayer_DrawPov` and `jkGame_Update`. Make that render operation a successful
  no-op in headless mode, consistent with adjacent render entry points.

The first repaired Windows x64 evaluation build has executable SHA-256
`58a21e4922aaaa2aefc11c9aefbb20cb186651012c62342137dc76c1dd6a23d3`.
It is a private evaluation artifact, not a released compatibility certification.

## Multiplayer acceptance

The initial isolated host comparison uses `JKM_MP` / `mdm02_freezer`, a stock
MotS deathmatch level, with these additional arguments:

```text
-headless -autostart -mp -episode JKM_MP -map mdm02_freezer
```

On 2026-09-08, separate SBX-only and Manowar-only runs each hosted for 60 seconds,
loaded their selected mod resources and stock map, reported MotS mode and
multiplayer mode, advanced gameplay time, and survived until harness termination.
The host socket was observed on UDP 27020. These are host smoke tests.

Fresh, separately seeded MP profile tests also passed for each mod: the normal
control and SBX function 67 / Manowar function 72 survived profile rewrite,
their activation functions were registered, and multiplayer simulation advanced
through the 35-second test window. Own-process window checks found no visible
game windows. These tests establish profile persistence, not activation effects.

Required follow-up gates include:

- Two clients joining/synchronizing; disconnect/rejoin and map transitions.
- Saved custom controls binding, reloading, and activating the intended action.
- SBX's three sabers, stance/evasion/throw/switch/color/trails/damage options,
  combat and input timing, and visual effects.
- Manowar's Jedi, Mandalorian, Alliance, and Vanguard personalities; jetpack,
  grapple, spy camera, teleporter, weapons, inventory and respawn behavior.
- Gameplay under load, object cleanup, longer sessions and teardown.
- Interactive UI/rendering/audio checks and comparison with the author's
  documented behavior. The initial autostart host fabricates player information
  and does not select all four Manowar personalities.

The upstream report about retail-created profiles remains independently open:
[OpenJKDF2 issue 369](https://github.com/shinyquagsire23/OpenJKDF2/issues/369).
No retail-profile format change is justified without an actual retail-created
profile oracle.

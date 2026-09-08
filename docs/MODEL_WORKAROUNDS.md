# Custom model compatibility and texture workarounds

This fork is the consolidated home for the animation-domain guard, the JKGM
PNG row-alignment helper, and the renderer correction discovered while building
nonstandard player models. The tools are source-only examples with synthetic
tests; they contain no retail models, animations, textures or generated art.

## Which findings also concern retail JK and MotS?

| Finding | OpenJKDF2 | Original retail engine |
|---|---|---|
| Added geometry must avoid animation-addressed node indices | Verified with stock JK/MotS animation data and actual OpenJKDF2 hierarchy observations. The guard reserves the complete declared KEY domain. | The numbered KEY/3DO contract and inspected stock animation files originate in retail JK/MotS. This is a general asset-authoring constraint, not a new OpenJKDF2-only feature. The guard's configured limits and modern runtime results do not certify an arbitrary model on a retail executable. |
| RGB replacement PNG rows can shear at widths such as 1254 | Reproduced in the inspected JKGM upload path; fixed here by setting tight unpack state around uploads and restoring it afterward. | This specific OpenGL JKGM PNG-upload path is an enhancement, not the original retail MAT renderer. No equivalent retail PNG defect is claimed. |
| Opaque RGBA can avoid that PNG row-layout problem without resizing | Asset-only fallback for older affected builds. It preserves every decoded RGB byte and both dimensions. | Not a retail MAT workaround; the original engine does not consume this JKGM PNG pack. |
| Direct multiplayer MPC model selection | One ordinary JK menu-to-host run accepted an edited MPC with the custom model already selected. Preview and gameplay paths are distinct. | Historical TDIR launcher/data evidence uses direct MPC model selection. It cannot make a mismatched rig animate correctly. No claim that every retail preview works or crashes. |

The original TDIR readme states that executable files are not modified. Its
Broly models use a 52-node size rig with visible geometry on the large-character
branch and custom KEY/PUP animation. In the modern stock-KEY test, the models
loaded but that visible branch remained rigid. This supports an asset/animation
explanation without identifying a required retail engine patch; it does not
establish complete TDIR compatibility. Shred18, the TDIR team and JKE are
historical precedents, not assets redistributed here.

## Animation-domain guard

See [the tool and examples](../scripts/model-workarounds/key-domain-guard/README.md).
The guard scans exact caller-selected KEY files or literal PUP references, then
reserves every declared animation slot before adding rigid detail children.
Stock data examined here addresses index 18 in several saber/player animations;
using that index for visible detail can give it the wrong animation. The safe
boundary comes from the intended corpus, not a universal hard-coded 19.

The JSON hierarchy validator trusts exporter metadata. It does not parse full
3DO geometry, establish correct pivots, or prove in-game appearance. Add KEYs
selected outside PUP files explicitly. Changing a weapon or mod can change the
reachable corpus and invalidate a previous reservation.

## Renderer correction and asset fallback

The PNG loader copies tight unsigned-byte rows. For width 1254, RGB rows are
3762 bytes, whereas the default four-byte unpack alignment advances by 3764.
The resulting source-offset error accumulates across rows. A 1254-wide opaque
RGBA image uses 5016-byte rows and avoids that discrepancy without resampling.

`JkgmScopedPixelUnpack` now captures the caller's unpack alignment, row length,
skip-pixel and skip-row state; sets them to 1, 0, 0 and 0 for the replacement
uploads; and restores the captured values on every exit. The scope covers
albedo, emissive and displacement upload paths. It leaves pack/readback state,
channel order, alpha handling, geometry, texture size and material matching
semantics unchanged.

The [PNG helper](../scripts/model-workarounds/png-alignment/README.md) remains
useful for older builds. It reports by default; an explicit new output path
creates an opaque RGBA copy and verifies pixel identity. It does not overwrite
images or strip existing alpha. See its README for accepted PNG formats and
filesystem behavior.

The original asset-only correction was observed with an unchanged Windows
engine under Wine64/Xvfb/Mesa llvmpipe. Its sanitized before/after record is
[included](../scripts/model-workarounds/png-alignment/observed-case.json).
It is distinct from testing this newly integrated renderer patch.

## Validation

```sh
python -m pip install -r scripts/model-workarounds/png-alignment/requirements.txt
python -B scripts/model-workarounds/run_tests.py
```

The real OpenGL regression uses a private surfaceless EGL pbuffer and synthetic
pixels. It includes an unprotected negative control, 128 exact byte-readback
cases across RGB/RGBA/BGR/BGRA, widths including 1254, incoming alignments 1/2/4/8,
non-default row/skip state, early-return restoration, and unchanged pack state:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc \
  src/Tests/jkgm_pixel_unpack_gl_test.cpp -lEGL -lGL -o /tmp/jkgm-unpack-test
LIBGL_ALWAYS_SOFTWARE=1 /tmp/jkgm-unpack-test
```

This requires desktop OpenGL and EGL development packages plus an EGL
surfaceless driver, such as Mesa. It does not open a game or desktop window.
CI runs this test in addition to both Python suites. Runtime/graphics outcomes
must retain their actual environment and scope; this document does not claim
native GPU performance, arbitrary-mod compatibility or complete retail parity.

## Sources, provenance and credit

- [Khronos pixel-storage reference](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glPixelStore.xhtml).
- [Inspected upstream JKGM path at 0dbd1ea8](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Platform/GL/jkgm.cpp#L719).
- [TDIR on Massassi](https://www.massassi.net/levels/files/2727.shtml) and [Jedi Knight Enhanced](https://www.massassi.net/levels/files/3087.shtml).
- [Shred18's historical mesh-budget discussion](https://forums.massassi.net/Editing_Forums/Jedi_Knight_and_Mysteries_of_the_Sith_Editing_Forum/thread_34814_page_1.html).

The tools were consolidated from `jkdf2-key-domain-guard` commit
`6059455a768d589cfea841eb1cc3752f3d786419` and `jkdf2-jkgm-png-alignment`
commit `8124edabd258b1f2deac6004d92475ad07c87e48`. Their MIT licenses and
synthetic evidence remain alongside them. This is original implementation and
validation work, without a claim that animation-slot reservation or OpenGL
pixel-storage correction was first invented here. Credit for original fork
work follows the main README: Andrei Kiralv/Darkahn and Coda Lilitari (OpenAI
Codex Interlocutor); upstream and historical contributors retain their credit.

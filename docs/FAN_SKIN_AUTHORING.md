# Fan skin authoring: retail, patched retail and OpenJKDF2

This September 2026 audit compares actual TDIR, Jedi Knight Enhanced, current
Shred-authored MotS+ models, and three older fan skin packs. It is an archive and
source audit. It does not add new game-runtime passes to the earlier validation
record. The repository contains code and technical findings only; downloaded
models, textures, screenshots, game data and generated art stay outside Git.

The main conclusion is that much better art is possible within these engines.
The useful techniques are deliberate topology, coherent UV atlases, material
choice, perspective texturing, rigid detail children, cutouts and real LODs.
More polygons alone do not produce those improvements.

## What the fan authors actually did

| Example | Measured implementation | Compatibility implication |
|---|---|---|
| TDIR transformed Broly | 1,348 polygon records, 15 meshes, 52 hierarchy nodes; largest mesh 408 faces; two 256-square indexed textures | The visible large-character branch uses custom 52-joint animations. Direct MPC selection does not make stock keys animate that branch. |
| JKE Kyle | 3,089 faces, 17 meshes, 19 nodes; largest mesh 443 faces; three 256-square RGB565 atlases | Detailed geometry is distributed within the existing per-mesh budget, with an added hair child. |
| JKE Boba / Boc | Boba 3,324 faces and 22 nodes; Boc 2,941 faces and 25 nodes | Cape, jetpack, belt, skirt and flaps are descendants of established torso/hip/head joints. These are rigid attachments, not cloth simulation. |
| Shred Vornskr in MotS+ 5.11 | 6,173 polygon records, 31 meshes, 32 nodes; largest mesh 263 faces; one 2048-square RGB565 atlas | Original 27-node numbering/parent/position contract plus teeth and spike children. The installation depends on MotS Remastered. |
| Shred Sith Undead in MotS+ | 4,406 polygon records, 20 meshes, 21 nodes; one 2048-square RGB565 atlas | Original 18-node numbering/parent/position contract plus jaw/torso partitions. Pivots/rest rotations are not completely identical. |
| Anime_Shinji Azaka / Kamidake | 18 nodes, four geosets; six cape faces inside the existing torso mesh | Two-sided indexed cutouts can create torn silhouettes without another animation joint. |
| Cave_Demon / MerKer 16-bit Xan | 18 nodes, four geosets; 11 RGB565 materials and one indexed transparent material; no KEY/PUP/COG | The author specifies basic JK plus hardware acceleration and accepts a poor selection preview. RGB565 itself is not an unofficial-patch requirement. |
| Anthony Piggott BoaM carried-Jan model | A 19th attached node and a 19-joint death KEY, with selected PUP routes replaced | Selective custom animation is possible as data, but a shared player PUP replacement affects other models too. |

These face totals describe authored polygon records; they are not a claim that
every sampled fan face is triangular. Primary author packages:
[TDIR](https://www.massassi.net/levels/files/2727.shtml),
[JKE](https://www.massassi.net/levels/files/3087.shtml),
[MotS+ 5.11](https://www.massassi.net/levels/files/3157.shtml),
[Anime Characters](https://www.massassi.net/levels/files/684.shtml),
[Xan](https://www.massassi.net/levels/files/1175.shtml),
[BoaM](https://www.massassi.net/levels/files/1007.shtml).

TDIR's control archive changes 31 entries of the original default palette.
Transformed Broly/USS Broly have face extra light 1.0 throughout; normal Broly
has 0.0. Those are total-conversion art/animation decisions. The original readme
says executables are not modified, and the inspected archive/launcher did not
identify an engine-patch dependency. This supports an asset-level explanation;
it is not an instrumented proof of every historical execution path.

JKE uses ordinary Gouraud lighting, generally zero extra light, and perspective
texture mode 1. Its included December 2006 patch enlarges world/resource/COG/
material limits and relaxes texture sizing, but does not document larger
per-mesh or puppet-node limits. Some wider JKE resources have larger textures
even though the selected player atlases are 256-square. A package-wide patch
requirement is not proof that every isolated skin needs the same patch.
[Shred's own patch instruction](https://forums.massassi.net/Main_Massassi_Forums/Discussion_Forum/thread_60213_page_1.html).

MotS+'s installation heading "VANILLA" first requires MotS Remastered 4.0.
Its large atlases are therefore not evidence for pristine retail support.
Its release notes also correct accidentally fullbright models. Shred's ordinary
lighting response should not be replaced with a universal fullbright setting.

The three older fan ZIPs contain 47 parsed models and 430 MATs, with no parse
errors. All preserve the first 18 node names/parents; 43 preserve the original
type masks. They demonstrate format techniques, not arbitrary high-node safety.
BoaM explicitly reports poor software appearance for some textures. There is no
universal historical cure for every software-renderer color problem.

## Limits must be named separately

Original JK 1.01 and the inspected original MotS executable admit up to 512
vertices, 768 UV vertices and 512 faces per mesh, with at most 24 corners per
face. Their animation caches have 64 node slots per track. The current inspected
fork retains those mesh checks in `src/Primitives/rdModel3.c` and the 64-node
definition in `src/engine_config.h`. No compile-time exception removes those
three mesh checks from the inspected loader.

Shred's historical 500-triangle workflow recommendation is a useful margin,
not a replacement for executable-specific checks.
[His 2005 post](https://forums.massassi.net/Editing_Forums/Jedi_Knight_and_Mysteries_of_the_Sith_Editing_Forum/thread_34814_page_1.html).
The [54-mesh discussion](https://forums.massassi.net/Editing_Forums/Jedi_Knight_and_Mysteries_of_the_Sith_Editing_Forum/thread_33977_page_1.html)
contains corrections to earlier guessed limits. Forum anecdotes about huge
total polygon counts are not a reproducible safety ceiling.

With this hierarchy representation, each additional separately drawn mesh
needs a node reference. Splitting every small detail indefinitely therefore
runs into the animation domain. The current high-detail prototypes already
use 60-62 nodes. Stock KEY allocation and node-cache capacity are different
constraints: reserving stock-addressed slots prevents accidental animation of
detail, but does not repair an original-engine read beyond a short KEY allocation.
See [the existing validation limits](MODEL_WORKAROUNDS_VALIDATION.md).

Raising a parser number alone is insufficient. A future larger-mesh fork change
must audit transform/projection and lighting scratch storage, allocation/error
cleanup, face indices, clipping, draw-cache batching, model collision and the
profile preview. A larger animation domain would additionally require updating
every track/cache and KEY path. Keeping 64 nodes while increasing per-mesh
capacity is a narrower first experiment; it is not implemented by this audit.

## Three detail tiers, with real requirements

Design and compatibility are separate axes. Preserve Specter, Revenant,
Abyssal, Ribbed Shroud, Organic Hybrid and Refined Hybrid as distinct designs
in every tier. The skirts are part of their designs. A technical upgrade does
not replace a skirted design with the exposed-leg Refined model.

| Tier | Planned gain | Required capability and acceptance |
|---|---|---|
| Retail | Better topology and texture painting; multiple legal 256-square tiles; a deliberate indexed fallback and native RGB565 hardware option; cutout hair/cloth; real distant LODs | Fit original mesh/node limits. Test actual original JK/MotS, chosen KEY corpus, colored/dark maps and profile/gameplay separately. Existing sampled runtime passes remain evidence for their exact frozen files only. |
| Patched retail / Enhanced | More anatomical and silhouette detail within measured mesh budgets; coherent 1024-2048 atlases where the exact patch/renderer supports them; carefully authored secondary animation if needed | Name the exact patch and graphics path that enables the larger material/resource budget. Verify it rather than infer support from JKUP/JKE/Remastered branding. A 16-bit texture alone does not justify the label. |
| OpenJKDF2 / Ultra | Higher-resolution direct-color replacements, finer mane/fingers/face/cloth, and a denser sculpt where a verified larger-mesh implementation permits it | Name the minimum tested fork build. Current PNG support can improve texture fidelity, but current mesh/node checks still apply. Above-budget geometry needs the explicit code work and downstream tests described above before it is a playable deliverable. |

The first useful art pilots are Abyssal and Ribbed Shroud: preserve their long
skirts, rebuild the mane as coherent layers, and put additional detail into the
face, teeth, fingers and rib structure. Enhanced should spend its texture budget
on a coherent atlas. Ultra should use genuinely finer sculpted forms and surface
detail, not merely subdivide the same silhouette or enlarge a noisy image. Exact
polygon and GPU budgets remain outputs of the prototype/scene benchmark.

For native MATs, UVs are in texels. Keeping an atlas layout while changing
256 to 2048 requires scaling UVs by eight; repacking requires per-island remapping.
External PNG replacements have their own scaling path. Multi-tile 256-square
RGB565 authoring is a demonstrated original-era alternative.
[Ruthven's author discussion](https://forums.massassi.net/Editing_Forums/Jedi_Knight_and_Mysteries_of_the_Sith_Editing_Forum/thread_35901_page_1.html),
[Jon`C on model palettes and texel coordinates](https://forums.massassi.net/Main_Massassi_Forums/Discussion_Forum/thread_67793_page_8.html).

Real geoset LODs are especially useful for multiplayer: JKE Kyle reduces
3,089 → 2,541 → 1,692 → 737 faces; Xan reduces 334 → 244 → 199 → 76 triangles.
Texture mipmaps do not reduce geometric workload. LOD mesh names/order and
hierarchy remain consistent while geometry is simplified. Actual multi-player
scene performance must be measured on the target GPU; a card model is not a
performance result.

## Immediate authoring correction, with limited claims

All six frozen prototypes have texture mode 0 at both mesh and face levels.
The [new helper](../scripts/model-workarounds/texture-mode/README.md) can create
a separate copy with both set to 1, preserving every other byte across all LODs.
It follows the documented original format and fan-author precedent. It does not
require an engine patch and is not a novel exploit.
[Massassi texture-mode specification](https://www.massassi.net/jkspecs/3do.htm).

Six local candidates were produced and structurally read back; geometry, UVs,
normals, hierarchy, materials and lighting match their inputs exactly. They are
loose diagnostic 3DOs, not new detail-tier packages or runtime-tested releases.
Their original packages remain untouched. Moving/oblique views are the relevant
next runtime comparison. Perspective mapping is not a proven uniform-color fix.

The independent indexed-material audit reproduced the exact top-level palette
indices from all nine unique frozen material contents. Source skin mean RGB is
109.757/93.026/80.073; stored indexed mean is 109.382/92.943/80.235. Source bone
is 111.880/83.522/62.098; stored indexed is 111.745/83.339/62.014. These inputs
are already brown. Mean absolute channel errors are about 15/255 for skin and
18/255 for bone, reflecting local quantization/dither variation despite similar
average color. This narrows the investigation; it does not resolve the complete
in-game orange appearance or remove the need to improve the art.

Calculated uniform CMP lighting rows differ more than direct base-palette
substitution, but are still only diagnostics. Actual active palette state,
vertex/sector lighting, gamma and capture conversion have not been reconstructed
by this calculation. No screenshots were recolored to manufacture a fix.

## Validation and provenance

The new helper has six synthetic tests: both mode levels/all geosets, unchanged
reporting, byte preservation, explicit reverse/idempotent operation, malformed
tables and refusal to overwrite. Combined Python suites: 55 tests discovered,
54 passed and one existing symlink test skipped on this Windows host because
symlink creation privilege was unavailable. No C/C++ engine code changed in this
audit, and no new engine/GPU/runtime result is claimed.

Locally retained provenance includes exact ZIP/member hashes, author readmes,
the 47-model unpatched audit, material/mesh and hierarchy comparisons, source
forum pages, palette calculations and six candidate hashes. MotS+ was fetched
as selected HTTP ZIP ranges: the selected GOO's CRC passed and its SHA-256 is
`bd80cac9e6f2655ddbb0978af6aaff883444a32aa87fcf013b863bccdf645f92`.
The full 841,419,133-byte ZIP was not downloaded, so no full-ZIP digest is claimed.
Seven unrelated MotS+ models rejected by the strict text parser are excluded
from the numerical conclusions, not described as corrupt game files.

No higher-detail tier is represented as completed by this document. No
third-party art, owned game material or generated model is redistributed here.

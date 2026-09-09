# MotS lighting pipeline audit and repairs

Date: 2026-09-09. Audited baseline: `4699f64ab5719a0efc4d31e2759b433308e85e98`.
This is a source audit of this OpenJKDF2/OpenMotS reimplementation, followed by
original synthetic CPU regression tests. It is not an audit of original
LucasArts source and does not establish complete retail visual parity.

## Stored channels and rendering modes

`src/World/sithSurface.c` preserves the four values on a MotS surface corner as
four planar blocks: mono, red, green and blue. A scalar row populates all four
blocks with that scalar and clears `SITH_SURFACE_1000000`; a four-value row sets
that flag. `src/World/sithArchLighting.c` similarly reads four independent arrays
for every architectural object mesh. Mono is an alternative to RGB, not alpha
and not a multiplier to apply to RGB.

`sithRender_RenderSectors` selects RGB when accelerated rendering and render flag
`0x80` are enabled. Software rendering selects mono. Ordinary things select mono
in `sithRender_RenderThings`; level-geometry model things are drawn in the sector
pass and may use architectural mono or RGB. The deferred-alpha pass now explicitly
selects the same world lighting mode as the opaque sector pass.

Level geometry requires the resolved `thingflags` bit `0x8` (`SITH_TF_LEVELGEO`)
and a `RD_THING_MODEL3` renderable. An ARCH index alone does not make a thing
eligible. Template inheritance copies the parent's flags; a child or placed
`thingflags` parameter replaces the whole hexadecimal value rather than ORing
with it. Ordinary models remain mono even in a MotS world using RGB.

For a Gouraud **world surface** corner, let `S` be the selected static component,
`D` the scalar dynamic light at the world vertex, `E` face extra light, and `F`
the ambient floor. The implemented order is:

1. Select mono or the three RGB components. A scalar-only row supplies the same
   value for all three components in RGB mode.
2. Compute `C = clamp(S + D, 0, 1)` for each component.
3. Interpolate those components independently when clipping the polygon.
4. Compute `Q = max(clamp(C + E, 0, 1), F)` in the hardware cache.

World static light uses the **local corner index**; dynamic light uses the
**global world vertex index**. The model RGB primitive instead indexes both its
static and dynamic arrays by the **model vertex index**. These layouts are not
interchangeable; the alpha pass therefore uses `rdPrimit3_ClipFaceRGBLevel`, even
when the sphere shortcut reports that the face is fully inside the frustum.

With render options bit `2` enabled, the world floor is
`clamp(sector.extraLight + globalExtra, 0, 1)`. It excludes sector ambient light.
For model faces, the floor is the camera ambient light, normally
`clamp(sector.ambientLight + sector.extraLight + globalExtra, 0, 1)`.
Disabling that render option makes the cache floor zero. Infrared lighting has
separate camera-ambient overrides and is not covered by the new visual contract.

After these repairs, Gouraud **model** light uses the architectural mono array
or scalar 3DO vertex base plus dynamic light in mono mode. RGB mode calculates
dynamic-only scratch, then adds the architectural RGB arrays or scalar 3DO
fallback exactly once. Dynamic light remains scalar, including point-light and
MotS spotlight calculations; this change does not add a colored dynamic-light
system. Ordinary scalar per-vertex variation is preserved.

## Confirmed defects repaired

| Path | Baseline defect | Repair |
| --- | --- | --- |
| Deferred alpha world faces | Always selected scalar intensities and scalar clipping; an intervening thing pass selected mono. Scalar uniformity optimization could discard distinct RGB. | Explicit world mode/array selection, correct RGB world primitive and RGB-preserving optimization guard. |
| World mode transitions | Accelerated sector pass set RGB when enabled but did not explicitly clear it when disabled. | Assign the requested mode on every sector pass. |
| Solid-material Gouraud RGB | Scaled normalized light by 63, then multiplied it by 255 again; palette red was added to all channels. | Keep RGB normalized until byte conversion and use the respective red, green and blue palette components. Initialize the GL light/luminance attribute for this path. |
| Architectural mono binding | Replaced the separately allocated scratch pointer with the shared model base pointer; draw writes corrupted that base, the original scratch allocation leaked, and model cleanup freed the aliased pointer twice. | Complete per-draw snapshot/restoration of the base/RGB pointers, with the original scratch allocation untouched. Restore after successful and unsuccessful draws. |
| Architectural binding bounds | Accessed arbitrary object/mesh/vertex indices and all four geosets without checking the model's declared geoset count. | Validate declared geosets and all LOD mesh/vertex coverage before binding. Invalid architectural bindings fall back to model lighting; allocation or malformed model-container failures skip the draw safely. |
| Placed architectural index | The thing loader assigned the positional ARCH index before copying the template, which overwrote the index with the template's default `-1`. | Apply the placed index after template inheritance, before initialization and parameter overrides. Guard the architectural field access for builds without `JKM_LIGHTING`. |
| MotS vertex-light calculation | Every vertex started from the first scalar base because the input iterator was not advanced. | Advance the base for each vertex; support a null base for dynamic-only calculations. |
| Model RGB accumulation | Calculated base-plus-dynamic scratch and then added a static RGB/base component again. | RGB requests dynamic-only scratch; mono retains base-plus-dynamic behavior. |
| Clipped solid model RGB | Stored pointer differences in 32-bit integers, risking invalid writes when arrays were far apart on a 64-bit process. | Use typed output iterators, as the other RGB paths already do. |

Architectural validation allows lower LODs to use a prefix of the architectural
mesh/vertex arrays. It checks coverage of every array actually accessed, not
semantic equivalence of a replacement 3DO's topology. Editor checks of model
names and exact topology counts are a separate protection; this audit does not
establish a model-content hash guard in the current .NET editor. The draw
scope does not turn model arrays into owners of world architectural arrays.

## Palette and material behavior

Let `P[index]` be a palette RGB byte triple, `L[row,index]` the 64-row palette
light table, and `s` a solid material's palette index. Before optional colormap
tint, global color effects and blending, the corrected hardware solid path is:

- Mode 0, fully lit: `P[s]`, ignoring extra light and floor.
- Mode 1, not lit: use `Q = max(clamp(E,0,1),F)` and return
  `P[L[floor(63*Q),s]]`.
- Mode 2, diffuse: use `Q = max(clamp(diffuse + E,0,1),F)` and the same mono table.
- Mode 3, Gouraud mono: use the component formula above and the same mono table.
- Mode 3, Gouraud RGB: each output byte is
  `uint8(clamp(255*Q[channel] - 0.5 + P[L[0,s]][channel], 0, 255))`.

The `-0.5` and conversion/truncation order preserve the existing implementation's
byte conversion; matching its exact rounding to retail remains a comparison task.
RGB solid shading is additive over the palette's darkest-row baseline, unlike
the textured RGB path. A solid material whose palette index is zero is not a
texture colorkey: `TEX_MODE_TEST` uses white sampled color and the computed vertex
color. The fragment shader's index-zero discard is in the paletted texture path.
The face translucency flag still selects alpha `90/255` when supported.

The modern OpenGL textured shader preserves RGB vertex color and multiplies it
by the sampled palette/16-bit color. Its palette light table also feeds a separate
emissive/bloom path through an RGB luminance estimate. This is not proof that the
modern shader matches retail's indexed-color software lighting, gamma, emissive
colors, filtering or postprocessing. A mono preview using the CMP ramp and an RGB
preview using palette color times independent RGB are useful explicit modes;
they should not be described as pixel-identical to every engine backend.

In particular, textured mono in this OpenGL implementation uses grayscale
vertex modulation of the sampled palette color; it does not replace the main
color with a CMP light-row lookup. Solid mono does use that lookup. Face flags
`0x4/0x8` are declared as texture clamp X/Y but are not applied by this OpenGL
path: game texture upload uses `GL_REPEAT` for both axes. These are source
observations about this backend, not proof of original retail or IJIM behavior.

## Validation and reproducibility

Run the new asset-free harness using GCC or Clang with sanitizer runtimes:

```sh
python3 src/Tests/run_mots_lighting_test.py --cc gcc --sanitize
```

The Windows x64 workflow runs this command before the full cross-build. Locally,
WSL Ubuntu GCC 15.2.0 passed **1,185 checks** with AddressSanitizer,
UndefinedBehaviorSanitizer and leak detection: 823 lighting checks, 193 thing
loader checks with `JKM_LIGHTING`, and 169 without that feature. Seven
old-behavior controls fail
independent output/ownership assertions: alpha mode, solid scaling, palette
channels, architectural aliasing, constant first-vertex base, doubled RGB base,
and the placed ARCH index overwritten by template copying.
The installed Clang 21 executable lacked its sanitizer runtime archives; GCC was
used instead, without installing or changing the host toolchain.

Tests execute the complete extracted production alpha pass, world and model RGB
primitives, both RGB polygon clippers, architectural draw scope and vertex-light
calculator. The model's actual calculator call is also extracted, so removing the
dynamic-only selection fails. Solid-cache normalization and encoding are exact
contiguous blocks from `rdCache_SendFaceListToHardware`, executed in a small wrapper.
Camera projection, draw submission, unrelated scalar/non-Gouraud clipping and
vector operations are explicit fixtures/stubs, not a full engine ABI or GPU test.
Every extracted production input is hashed and checked unchanged after the run.

The additional loader harness executes the complete production static-thing
loader, reset, template-copy, world initializer, position assignment and argument
tokenizer, plus the exact `thingflags` parameter case. Its text fixtures cover
positive and zero ARCH indices, `-1` and other negatives, absent indices, a named
parameter in the first optional position, parameter overrides, template
inheritance/isolation, rejected sectors and repeated load/cleanup. Both feature
configurations compile; the non-`JKM_LIGHTING` fixture has no ARCH field.
Line IO, resource/network calls, unrelated parameter dispatch, world
postprocessing and rotation math are explicit stubs. These tests do not claim
validation of every thing parameter or malformed-input recovery.

Coverage includes all six frustum planes in textured and solid RGB variants,
nonsequential world/model indices, independently varying channels, scalar fallback,
mode toggles, distinct mono bases, zero and multiple dynamic lights, palette
baseline channel differences, clamps/floors, multiple LODs, bad architectural
indices/counts/null arrays, repeated alternating draws, failed draw/allocation
paths and separate model allocation destruction. No game, editor or visible
window is launched by the harness.

Full Windows build validation uses the documented MinGW CMake toolchain and the
pinned submodule revisions in an isolated local source snapshot, because the
existing external-project build patches dependency working trees. The full
Windows x64 target and the final incremental rebuild including the placed ARCH
loader repair both passed with MinGW GCC 13-posix. The resulting
`openjkdf2-64.exe` is 15,994,670 bytes, SHA-256
`ba5073793bd8d5301e72d3a7006fc34d5156d312c13fddd88ba0cf8f256ebc8d`.
This is the baseline plus an uncommitted lighting patch, not a clean release
build. The receipt and logs are retained locally under the ignored
`build_win64/lighting-audit` directory. The binary was not executed.
The final receipt distinguishes tested snapshot hashes from working-tree hashes:
four untouched production inputs have Git-checkout CRLF/LF differences only,
verified equal after the same newline normalization used by the extraction
harness. Every changed source/test file matches the snapshot byte for byte.

## Remaining limits and next comparisons

- `sithArchLighting_ParseSection` still accepts unchecked counts, read failures
  and non-finite values and lacks transactional allocation cleanup. Render-time
  bounds checks cannot make malformed input safe before it reaches rendering.
- Mesh lighting mode 6 overwrites face extra light with mesh extra light, then
  applies the face's lighting mode. The existing path does not calculate ordinary
  Gouraud scratch in that mode; exact animated/script-driven mode-6 behavior
  remains unverified. A static preview must label any 6-to-3 approximation and
  still honor lower face lighting modes.
- No retail executable comparison, in-game image capture, authored-world traversal,
  animated-model test, lighting bake, spotlight image comparison or save/load
  malformed-input fuzz campaign was performed here.
- OpenJones3D's later-engine RGB/RGBA types are useful structural references, but
  they are not MotS's independent `[mono,R,G,B]` file contract. Independent editor
  bake/reference research belongs to the companion editor audit.

The pre-existing untracked `docs/AGENT_CONTROL_PROPOSAL.md` was read for context
and left unmodified. No commit, push or publication is performed by this audit.

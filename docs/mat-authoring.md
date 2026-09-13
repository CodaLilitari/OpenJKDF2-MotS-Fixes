# MAT authoring in the native OpenGL renderer

These findings concern the source snapshot at
`a5312aa42c33b7e3428f6fcff1934a5c38bb11b5`. They document existing compatibility
behavior; they are not a new material format or a new retail engine patch.
No model, texture, game data or capture is distributed with this note.

## Large MAT dimensions and the UV domain

`std3D_GetValidDimension` in `src/Platform/GL/std3D.c` returns the smaller of the
selected texture dimension and 256. `src/Raster/rdCache.c` uses that returned
dimension, shifted by the selected mip level, to normalize 3DO UV coordinates.
The ordinary GL upload still uses the material surface's actual dimensions.
Consequently, the normalization limit is not a 256-pixel GPU storage limit.

One existing fan-author precedent is Shred's Sith Undead in MotS+ 5.11: a 2048²
RGB565 MAT with one stored mip, paired with approximately one 256-unit UV period.
Its U coordinates span 0..256 and V spans -255.72..0. That package documents a
Remastered prerequisite; its presence does not prove pristine retail support.
These asset coordinates match the port's existing JKE compatibility convention;
this note does not claim a new technique or establish the author's intent.

Do not automatically multiply UVs by 8 when replacing a 256² MAT with a 2048²
MAT. In this path that changes atlas repetition. Determine the intended
renderer's effective denominator first; atlas repacking still requires explicit
island remapping. Retail DirectDraw, wrappers and external PNG replacements
must be assessed in their own paths.

The selected-mip interaction also matters. With a 1024 base MAT, selected stored
mips 0, 1 and 2 give normalization denominators 256, 512 and 1024. That predicts
changed mapping across distance transitions for a model expecting a fixed 256-unit
domain. This source-derived prediction has not been accepted as a reproduced
runtime defect. Use one stored mip for the initial oversized-MAT control and
test distance/motion before promising release quality.

## What the ordinary MAT loader accepts here

`rdMaterial_LoadEntry` reads the texture header dimensions and stored mip count,
allocates each SDL surface and reads its pixel payload. No explicit power-of-two
or 256-pixel rejection was found in the inspected native SDL/OpenGL material
loader. The GL upload uses those dimensions. This is not a claim about every software/retail path.

The loader reads `pitch * height`, so payload row alignment still needs care.
A 1254-wide RGB565 row is 2508 bytes and is divisible by 4. That exact case was
tested; arbitrary odd-width RGB565 or unaligned indexed images were not.

Ordinary opaque RGB565 uses the established format tuple
`[1, 16, 5, 6, 5, 11, 5, 0, 3, 2, 3, 0, 0, 0]`. Selected JKE Boba/Boc, modern
Undead and the local comparison assets use the same tuple. Differences in opaque texinfo
words were observed, but no quality effect from copying those words was found
in the inspected loader/raster/GL consumers.

## Palette, mips, filtering and texture mode are separate controls

- Indexed MATs store palette indices. Native GL looks up the current world CMP
  colors. RGB565 and PNG bypass that indexed color lookup while retaining
  vertex lighting and later rendering effects. In the current fragment shader,
  the light-table-derived addition goes to the emissive output; it does not
  justify attributing every baseline color cast to the light table.
- Native GL still selects authored MAT mips using engine distance rules. Each
  selected mip becomes level 0 of a separate GL texture. Standard MAT and PNG
  albedo uploads both restrict the GPU texture to level 0. Enabling filtering
  does not create a full GPU mip chain or anisotropic filtering.
- Indexed bilinear filtering interpolates looked-up palette colors in the
  shader, not raw index numbers. PNG filtering selects linear/nearest directly.
  Ordinary MAT upload has a timing caveat: it checks the surface's 16-bit flag
  before assigning that flag. The later world-load settings update should
  correct preloaded surfaces, but a late upload can initially use nearest.
  This is a source finding, not a newly measured sampler defect.
- Historical 3DO texture modes 0 and 1 denote affine/perspective. No corresponding
  native GL sampling branch was found in the inspected path. The GL shader's
  similarly named `tex_mode` instead derives from color format/filtering. A
  historical software-renderer setting is not automatically a GL quality lever.

## How to isolate MAT fallback in the render probe

Set the local profile cvar `r_bEnableJkgm` to false before each fresh process.
The cvar is marked read-only for runtime setters, but profile JSON loading
writes its linked value directly after loading legacy preferences. Already-loaded surface
caches precede replacement selection, so toggling during an existing run is
not an equivalent test.

Keep geometry/UV bytes, actual camera, pose, within-game lighting, precache and
rendering settings fixed. Compare these distinct delivery paths: 256² indexed
MATs with four mips, 256² RGB565 MATs with four mips, full-size PNG replacements,
and full-size RGB565 MATs with one mip. The PNG/full-size comparisons change resolution as well as format or mip count.

The probe's positive upload records describe JKGM PNG allocation only. For
MAT-only tests, preserve exact input hashes, disabled replacement configuration,
successful captures and absence of PNG uploads. Do not rename that evidence
as individual MAT GPU telemetry. The global filter field likewise does not
report every texture's actual sampler. The private PLR may be normalized during
ordinary startup; treat it as writable configuration while preserving original
source files and staged immutable assets.

## Bounded native observations

On 2026-09-09, eight private Windows cases completed: four paths in JK mode and
the same four in MotS mode, each with three 1920×1080 captures. Hardware was RTX
5070 Ti with NVIDIA 610.62, native OpenGL. Gamma 1, texture filtering/precache on,
SSAA 1, bloom/SSAO off, one fixed stock-idle pose. The tested executable SHA-256
was `de842e67b351f8b5cb0765d23537ea4e4fb8527db94910bc388e413c0e7cd8dd`,
attested to the source snapshot named above. No engine changes were needed for
the material experiment.

The native-size RGB565 candidate used seven 1024² materials and one 1254² face
material, each with one stored mip. Source image dimensions and original geometry/UV
bytes were preserved. Both game modes completed with zero recorded GL errors
and no replacement uploads. In the fixed subject rectangle, its maximum
absolute channel difference from the existing PNG replacement was 4/255 in each
game. The background control remained identical. This supports the tested
coordinate convention and close appearance at that view; it is not a universal
equivalence or an artistic-quality score.

These were static appearance subjects in a diagnostic MP host, not connected
player peers. The result does not establish original/patched retail support,
profile-picker behavior, moving/distant quality, crowd performance or networked
MP acceptance. The authoring lesson is to verify material delivery, then spend
detail where intentional UV mapping and the visible form can use it.

Primary references:

- [Massassi MAT specification](https://www.massassi.net/jkspecs/mat.htm).
- [Ruthven's multiple 256² 16-bit texture workflow](https://forums.massassi.net/Editing_Forums/Jedi_Knight_and_Mysteries_of_the_Sith_Editing_Forum/thread_35901_page_1.html).
- [MotS+ 5.11 release](https://www.massassi.net/levels/files/3157.shtml).
- [GL upload/normalization source at the tested snapshot](https://github.com/andreipemberton/OpenJKDF2-MotS-Fixes/blob/a5312aa42c33b7e3428f6fcff1934a5c38bb11b5/src/Platform/GL/std3D.c).
- [Cache/mip source at the tested snapshot](https://github.com/andreipemberton/OpenJKDF2-MotS-Fixes/blob/a5312aa42c33b7e3428f6fcff1934a5c38bb11b5/src/Raster/rdCache.c).

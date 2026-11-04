# GeoSith

An in-game level and 3DO editor built as a fork of [OpenJKDF2](https://github.com/shinyquagsire23/OpenJKDF2).
Supply your own Jedi Knight installation in `JK/`. No Jedi Knight game resources are included.

Windows x64 is the tested GeoSith platform. See [upstream documentation](README-OpenJKDF2.md) for OpenJKDF2 background and other platforms; GeoSith-specific support there has not been verified.

Clone with `git clone --recurse-submodules https://github.com/SuperDonRobb/GeoSith.git`.
The original license and third-party notices are retained; see [LICENSE.md](LICENSE.md).

Run **Launch GeoSith.cmd**, then choose **Create New Level** in single player
or on the multiplayer host screen. New levels contain one rectangular sector.
Single-player levels start with one walkplayer; multiplayer levels start with
four walkplayers spaced around the room. The launcher selects this project's JK installation.
New sectors start with sector light 0.5. Extrusions inherit their source sector light.
Placed lights add vertex lighting on top of that baseline; adding/removing lights
does not reset sector brightness. Sector light is saved and synchronized.

| Control | Action |
| --- | --- |
| F8 | Toggle editor / play mode |
| Normal movement and look controls | Walk and jump with collision in editor mode |
| Double-tap jump | Toggle flight on/off; collision starts enabled |
| Home | Teleport to the next walkplayer, wrapping back to the first |
| Q while flying | Toggle player collision |
| Tab | Toggle overhead sector map / 3D editor |
| Aim near a vertex, edge, or surface | Highlight it |
| 1, then middle mouse click | Choose Select or Clean Up mode |
| 1, then primary fire | Toggle the highlighted item in the red selection |
| 1, then double-click primary fire | Clear the entire selection |
| 1, then hold secondary fire | Move all selected items together |
| 1, then hold secondary fire and click primary fire | Extrude all selected surfaces together by one grid unit |
| 2, then middle mouse click | Open the searchable material thumbnail browser |
| 2, then hold secondary fire | Grab a surface and move its UVs with travel or mouse-look |
| 2, then mouse wheel | Cycle the current material |
| 2, then primary fire on a surface | Apply the current material |
| 3, then middle mouse click | Open the searchable object template browser |
| 3, then mouse wheel | Cycle the selected template |
| 3, then primary fire | Place the chosen template at the aimed collision point |
| 3, then hold secondary fire | Grab and move an object or walkplayer |
| 4, then primary fire | Place an editor light in front of the player |
| 4, then hold secondary fire | Grab and move a light on the editing grid |
| 4, then wheel while aiming at a surface | Adjust its sector light by 0.05 per notch (0-1) |
| 4, then wheel while aiming at a light | Adjust intensity by 0.1 per notch |
| 4, then Shift + wheel while aiming at a light | Adjust range by one grid unit per notch |
| 4, then Delete while aiming at a light | Delete the light |
| 5, then middle mouse click | Open Placed COGs; add scripts and assign exposed parameters |
| 8, middle mouse | Template creator: add/search parameters, browse 3DOs, save a template for Weapon 3 |
| 9, then primary fire | Enter the 3DO builder with a new draft |
| 9 inside the 3DO builder | Primary fire places the wireframe primitive; wheel cycles shapes; Shift+wheel sizes it; Ctrl+wheel rotates it; middle mouse opens the list |
| Escape inside the 3DO builder | Discard the draft or Save as 3DO to the model library; both remove its workspace preview |
| Hold secondary fire | Grab; player travel and mouse-look move the selection |
| 0, then hold primary fire on a surface | Start the saber cleave preview |
| 0 (saber), then Shift + primary fire | Cleave the highlighted sector into two adjoining sectors |
| Release primary fire | Split the surface along the preview line |
| Hold secondary fire, click primary fire | Extrude the grabbed surface outward by one grid unit |
| Hold secondary fire, then Shift + primary fire | Extrude horizontally to a vertical far face; also works on selected surfaces |
| Shift + primary fire | Copy a surface material |
| Ctrl + primary fire | Paste the copied material |
| Shift + hold secondary fire | Move the surface UVs by moving the player |
| Ctrl + Z | Undo, up to 30 edits |
| Ctrl + Y or Ctrl + Shift + Z | Redo |
| 1, then Delete on an edge or vertex | Dissolve it if the remaining geometry is convex |
| Delete while highlighting a surface | Delete its sector; Ctrl+Z restores it |
| Escape â Save Level | Name and save `JK/Episode/<name>.gob` |
| Escape â Level Header | Edit the level title, colormap, physics, sky, rendering and intro text |
| Escape -> Episode Editor | Edit episode.jk settings, sequence entries and loading strings |

Primary and secondary fire follow the player's bindings. Ctrl and Shift act as
modifiers in the editor; a fire binding on either modifier alone is ignored.
Assign fire to mouse buttons in the JK control menu if needed.
Editor mode starts with flight off. Two separate presses of your bound jump
control within 350 ms toggle flight. Holding jump does not toggle it. While
flying, jump ascends and crouch descends; double-tap jump again to resume gravity.
Weapon 9: primary fire starts a new 3DO; middle mouse opens the custom model
library, where you can edit or delete saved 3DOs. Editing opens a copy in front
of you; saving with the original name replaces the library model, while
Discard preserves it. Reopening needs free capacity for the editable copy.
Weapon 9 enters a separate 3DO workspace. The world turns blue while the current
model keeps its normal textured colors. A circle marks its origin. With weapon 1, aim at that circle and hold Fire 2
to drag the origin on the world grid without moving the mesh geometry. This
works through the mesh and supports undo/redo. 3DO cleave previews and cuts
snap to half the world grid, matching thing movement. Each placed
primitive adds one mesh. Only cube vertices snap during placement; spheres,
cylinders and pyramids keep their generated shapes. Moved vertices snap to the
world grid. Alt+mouse wheel cycles three sphere/cylinder detail levels.
While building, weapon 0 cleaves faces (hold primary fire, draw, release), weapon
1 selects faces/edges/vertices and secondary fire drags the selection, and weapon
2 paints materials or adjusts UVs with secondary fire. Hold secondary fire and
click primary fire on a face to extrude one grid unit along its positive/outward
normal. Movement is grid-snapped; mesh edits never perform level-sector CSG.
3DO cleave endpoints are connected to adjacent faces. Vertex dragging follows
connected edges; hold Shift for free movement. Dragging onto a connected corner
merges the vertices and removes collapsed faces.
Ctrl+Z / Ctrl+Y use the shared undo history and multiplayer host approval.
Escape opens a name dialog with Discard, Save as 3DO, and Save as _walkstruct.
Save as _walkstruct creates a same-name template based on _walkstruct and places
it exactly where you built the model. Discard removes the
draft. Save as 3DO keeps it in the level's reusable model library. Both choices
remove the workspace model from the level view. Weapon 8 opens the Template
Creator with middle mouse: select a created template to edit or choose New.
Saving an existing custom name replaces that template. The edit menu also has
Delete template. Remove placed things or dependent templates before deleting a
template or model they use. Choose the base from the
searchable template list, use + Parameter to search
all supported JK parameters, and edit their values. Clicking a model3d value
opens a searchable, textured browser of game and custom models. Save template
adds it to Weapon 3's object browser. Save Level includes standard 3DO 2.1 files
under 3do/<name>.3do, template definitions, and editable library metadata.
The builder currently supports 8 models and 16 total meshes per level, with 64
faces and 512 face-corner vertices per mesh, and 24 vertices per face. These
models are saved to the level's reusable library. Weapon 8 creates templates
from game or custom 3DOs, with a base template and editable JK parameters.
Saved templates immediately appear in Weapon 3. Save Level packages the models
and templates inside the GOB. Custom templates support undo and multiplayer;
all peers must use this version.
Multiplayer participants must all run this version because mesh data extends
the editor network format.

Releasing the movement controls while flying stops you immediately.
Turning flight off restores collision. Home uses the walkplayer positions and
orientations saved in the level; walking around does not move those spawn points.
In the overhead map, green lines show sector geometry on a black background and
gray lines show the grid. Vertices have circular handles. Left-click a vertex or
edge to toggle its selection; click other handles to add them. Overlapping handles
at different heights are picked together. Click inside a sector to select sectors
beneath the cursor, or left-drag a rectangle to select sectors. Selected geometry
is red. Right-drag moves the selection on the XY grid without changing its height.
Weapon 1 vertex, edge and surface selections are retained exactly when switching
between 3D and map views.
Press C to cleave selected sectors: click and drag a line, or click its two
endpoints. The snapped line previews yellow when it crosses selected sectors
and red when it misses. Completing the line cuts touched selected sectors
vertically and connects the halves with adjoins. Escape cancels the cleave.
Delete removes all selected sectors and closes former adjoins, keeping at least
one sector in the level. Each completed cleave or deletion is one undo step
and uses the multiplayer host approval path. Ctrl+C still copies sectors.
Hold Space and move the mouse to pan; use the wheel to zoom around the cursor.
Right-drag moves selected sectors on the editing grid, including their existing
objects, walkplayers, path frames and lights. Copying uses sector geometry only.
Ctrl+C immediately starts an unplaced preview; Ctrl+V recalls the clipboard as
another preview. Its yellow outline follows the cursor without changing the
level. Left-click places and merges it, preserving internal adjoins and closing
exposed boundaries. Escape cancels placement. Insert starts a preview of a new
default six-sided sector with the same placement controls.
Shift+left-click starts drawing a sector outline on the grid. Further clicks
add walls; click the first vertex to close an outline with at least three walls.
Backspace removes the last placed vertex so drawing continues from the previous
vertex. Escape cancels unfinished drawing. New sectors use the current material and a
height of one JK unit, starting at the floor height of the room occupied when
the map was opened. Only convex outlines are accepted. The live wall turns red when it would make
the outline non-convex or cross another wall, and clicks there are ignored.
Drawing no longer automatically splits concave shapes. Placed overlaps use the same rules as
3D edits. Ctrl+Z/Y undo/redo, including host approval in multiplayer. Player
movement, look, weapons and other 3D inputs are suspended until Tab returns to
the 3D editor. F12 screenshots remain available.

Editor status, tool statistics, object/frame labels and map text use font
a fixed warm-white tint, with glyph shading read through 01narsh.cmp and colors
remapped into the active palette. Changing the level
colormap updates this mapping without changing the level lighting or menus.

Highlights use thick outlined edges and circular vertex markers. The selected
vertex has a larger circle; markers use the world viewport independently of HUD
and crosshair settings. The 3D editor uses a compact yellow crosshair with a dark
orange outline, independent of the normal gameplay crosshair setting.
Weapon 1 keeps a selection of up to 256 surfaces, edges, or vertices, shown in
red. Press Enter with one or more surfaces selected to open Surface Attributes.
Edit the material name, U/V scale, geometry/texture/light modes, extra light,
and adjoin distance. Surface Flags, Face Flags, and Adjoin Flags open separate
checkbox dialogs, including unknown bits. Mixed values remain unchanged unless
edited; a `~` marks mixed flag bits. Only toggled bits are applied to all selected
surfaces. Apply updates the running level as one undo step; Cancel discards the
draft. Save Level writes the attributes and scaled UVs into the GOB. Weapon 1
can select adjoin boundaries; their connection number is read-only, while their
flags are editable. The opposite side is edited by selecting it too.
 Shared corners move only once during a group drag. Group extrusion requires
surfaces only; the new outer faces remain selected for further edits. Adjacent
extrusions connect through adjoins. The whole group operation uses one undo step.
Undo retains the latest 30 edits, including geometry, UVs, materials, deletion,
and header changes. A continuous drag counts as one edit. A new edit after undo
starts a new history branch and clears redo. History resets when loading a level.
Before a vertex or edge move, cleave endpoints are connected to every incident
face edge, including edges that previously omitted that endpoint. This keeps
door-frame junctions shared during welding instead of reconstructing extra
polygons around disconnected face loops.
Vertex welding checks the closed sector boundary, repairs collapsed/reversed
face patches, and keeps supporting face hulls free of interior vertices so
merging a corner cannot silently remove a wall triangle. Vertex dragging follows
connected edge segments and stops at the next vertex; redundant edge vertices
follow their boundary edge. Hold Shift while dragging a vertex to move it freely
in all directions. Shift still pans UVs when grabbing a whole surface.
With weapon 3, aim at a placed thing and press Enter to open Thing Attributes.
Edit its position, rotation, or thing flags. Numframes has + and - controls:
the first frame copies the thing's current pose, and additional frames start one
grid unit above the previous frame. The minus button removes the displayed frame;
use < and > to choose a frame or type its coordinates and angles directly.
Apply commits one undo step; Cancel discards the dialog changes.

Frames appear in edit mode as red translucent model copies with zero-based frame
numbers and rotation circles. In weapon 3, hold Fire 2 over a frame to move it,
or over its circle to rotate it. Movement snaps to the editor grid; rotation snaps
to 5 degrees and stays on the vertical axis unless Shift is held. Frame positions
are independent of the original thing. Saved levels include native JK numframes
and frame overrides; path movement is configured automatically, with no manual
thing-flag setup required. Up to 64 frames per thing are supported.
Weapon 3 reads the supplied `master.tpl` (also accepts `mater.tpl`). Its browser
shows textured 3D models, sprite thumbnails, or markers for templates without a
visual asset; the chosen template also appears at bottom right. Its section button
filters Sprites, Walkstructs, Projectiles, Pickups, Ammo, Weapon pickups, Enemies,
NPC, Dark Jedi, Ships, Elevators, Ghosts, Decor, Throwable, or Other. Sections can
overlap; Walkstructs includes descendants of `_walkstruct`. Placed objects
stay still while editing and resume template behavior in play mode. Walkplayer
spawn markers are visible in editor mode and can be moved independently of the
player. Every object and walkplayer has a green facing line with a circle on its
end. With weapon 3, hold Fire 2 on that circle and drag to rotate the object;
grabbing the body still moves it. Rotation keeps the center fixed, updates live,
and uses one undo step per drag. Rotation snaps to 5-degree increments and
changes yaw around the vertical axis by default; hold Shift to rotate freely. With weapon 3, Delete removes the hovered object or walkplayer; at least
one walkplayer is kept. Deletion supports undo/redo. Placement uses the model
hierarchy bounds (with radius as a fallback) to keep models outside solid walls
and floors, including corners. Positions snap to the geometry grid along the
target surface, with a small perpendicular offset for model clearance.
Up to 1024 objects and 32 walkplayers can be saved. Template definitions are
included in the saved GOB.
Weapon 2 shows the current material above the bottom-right ammo display. Its
browser lists materials from installed GOBs and loose material folders, with
search and paging. Click its section button to choose Models, Other, All, or a
numbered level group. Thumbnails preserve the source aspect ratio. Embedded MAT
palettes take priority; otherwise previews use colormaps identified from the
installed campaign's surface references, falling back to the default palette.
Run `tools/prepare_assets.py` again after installing additional asset packs to
refresh these associations. Saving includes references to any additional
materials used by the level.
With weapon 1, Delete on a highlighted edge merges its adjacent coplanar
surfaces when their union is convex. Delete on a vertex removes redundant
vertices, merging incident coplanar faces as needed. Deletions must leave closed,
planar, convex geometry; unsafe deletions leave the level unchanged. Shared
adjoins are updated on both sides, and surface COG references are remapped.
Merged faces keep the highlighted surface's material and settings. These edits
support undo/redo and the normal multiplayer host approval path.
Delete on a highlighted surface removes its owning sector and closes its former
adjoins. Walkplayers stranded by the deletion move inside a remaining sector;
the player also relocates if necessary. The final sector cannot be deleted.
Dragging compares consecutive crosshair ray intersections with a reference
plane parallel to the grabbed surface. The plane follows player travel so walking
past the original wall cannot lose the grab. Cursor motion stays independent of
live geometry rebuilds. Parallel or missed rays pause only cursor tracking;
player travel continues, and valid tracing resumes without a jump. Surface movement follows the strongest local
left/right, up/down, or normal/depth component. UV dragging follows both tangent
components together, with the visible texture following the cursor. With an empty selection, weapon 1 can directly grab the
highlighted edge or vertex. Coincident vertices weld to one shared position in
the live world and saved level, while retaining separate UVs at texture seams.
Hold Shift with the saber to highlight the entire sector. Start a cleave while
holding Shift, aim its line on a surface, and release primary fire to cut the
sector through that line. The two convex sectors connect through an adjoin.
Without Shift the saber continues to split a single surface.
Extrusion immediately adds one grid unit along the outward face normal and ends
the grab. Release secondary fire, then grab the new face to move it farther.
A harmless rail detonator sprite and sound mark the original surface center.
The grab distance stays fixed, so distant grabs respond more to mouse-look.
Edges and vertices follow combined movement with grid snapping. The grid is
1/16 JK unit (0.0625), about half the default player's width. Surface drags snap
along their selected local axis; vertex/edge handles and cleave guides snap to
the grid. Exact CSG intersections are retained to keep sectors sealed.

Textures use world-aligned planar UV projection at 256 texels per JK unit.
Moving geometry reveals more or fewer texture repetitions instead of stretching
the old UVs. Shift-grab adjusts the projection offset in grid-sized increments;
offsets survive later edits and save/reload. Multiple walkplayers and their
orientations are preserved when saving.
Existing editor levels receive the corrected texel density when entering the
editor; earlier world-mapping offsets are converted with it. Save to retain it.

Level Header has General, Rendering, and Intro Text pages. General includes the
display name, colormap chooser, gravity, ceiling sky height, horizon distance
and pixels per revolution. Rendering includes horizon/ceiling offsets, all four
mipmap and LOD distances, perspective distance and Gouraud distance. Intro Text
provides all 20 JK loading-screen text lines. Apply updates the working level;
Save Level writes the settings, title and intro strings into its GOB. The title
is independent of the save filename. Applying a colormap updates the running
level and its texture caches immediately, including when undoing the change.
Save Level makes it persistent. Indexed textures still need a compatible level
palette (the 01 set normally uses `01narsh.cmp`). JK format version stays at the
engine-supported 1.
JK's string format requires text without double quotes.

Convex sector subtraction removes overlapping volume. Opposing faces are split
at their shared intersection to create reciprocal adjoins. Vertex and edge moves
rebuild affected convex hulls when necessary; this can change face topology.
Planar cleanup moves preserve existing cleave partitions. Extra collinear
vertices on sector edges can slide along that edge and snap onto another
vertex in the same sector to weld. Degenerate faces disappear after welding;
cleaning ceiling-edge corners avoids unnecessary extrusion side walls.
Sector and surface storage grows with the level; there is no fixed 1024-sector
or 192-surfaces-per-sector ceiling. Geometry scratch buffers, map selections,
undo snapshots and network snapshots grow with their contents. Surfaces still
support at most 24 vertices. Placed objects and COGs use growing editor storage
up to the engine's ID/table limits, with room reserved for players and gameplay.
Snapshots allocate their actual encoded size. Multiplayer retains a 256 MiB
uncompressed document budget (including manifests/scripts) and a 512 MiB
compressed-transfer budget. Hosts and clients must use the same build.

Multiplayer edits and undo are synchronized through the host. Stock levels
can be converted for editing; saving a converted stock level uses a separate
custom episode rather than replacing the original game archive.

## Build

Visual Studio 2022 with the C++ desktop workload, a Windows SDK, CMake, Git, and
Python 3 are required. From this folder:

```powershell
python tools/prepare_assets.py
powershell -ExecutionPolicy Bypass -File tools/build.ps1
```

`prepare_assets.py` extracts resource references and templates from the local
`JK/Episode/JK1MP.GOB` and prepares the installation marker OpenJKDF2 requires.
Game assets and generated levels are excluded from source control.

The build includes the native geometry tests. To run them separately:

```powershell
cmake -S tests -B build-tests -A x64
cmake --build build-tests --config Release
.\build-tests\Release\editor_tests.exe
```

Tests exercise picking, cleaving, vertex/edge edits, extrusion, overlap removal,
partial portals, repeated portal rebuilding, UV density, level headers, validation,
and GOB output. Save/reload integration also checks title, all header settings,
the selected colormap and intro text.
`GEOSITH_SELFTEST=1` opts into the live rebuild integration test only when
running the generated `sithed_smoke` episode.
Run `powershell -ExecutionPolicy Bypass -File tools/smoke.ps1` for the geometry
tests, live engine test, and a reload of the exported two-sector GOB. Test logs
are written under `build/`.
The live test also exercises queued AI sound events and surface/sector animations
across repeated geometry replacement. Run
`powershell -ExecutionPolicy Bypass -File tools/stress.ps1 -Render`
to exercise 240 edits during normal rendering and
collision updates. The test verifies that frames were rendered; logs are saved
as `build/stress.stdout.log` and `build/stress.stderr.log`.

Upstream engine: https://github.com/shinyquagsire23/OpenJKDF2
The upstream license is in `engine/LICENSE.md`.

Edit mode honors the normal screenshot binding (F12 by default), capturing the
finished frame including editor highlights and HUD.

To create optimized stairs, select two opposite vertical end walls of the same
six-surface sector with weapon 1: select the high end first and the low end
second. Press Enter, set **Stair steps**, and click **Create Stairs**. The floor
and ceiling must be horizontal. The default uses approximately one grid unit of
rise per step; the step count is no longer tied to a fixed sector/surface storage capacity.
The tool cleaves a floor-to-ceiling diagonal ramp, retains the air wedges above
each visible tread, and removes the volumes below the treads. The invisible ramp
adjoins are Floor + Impassable, with visibility/sound enabled and movement through
the ramp disabled. Walking uses the smooth ramp while the visible geometry shows
steps. Creation is one undo step; Save Level keeps the new sectors and flags.

Weapon 4 places editor point lights in growing storage (default intensity 1, range 4 JK units). Light markers and the hovered light's intensity/range appear in editor mode. Lighting updates immediately with distance falloff, surface angle and solid-wall occlusion; adjoins pass light. Sectors start at light 0.5. Aiming at a surface shows its wireframe and sector brightness; the wheel adjusts that sector in 0.05 steps from 0 to 1. Placed lights add Gouraud vertex lighting without resetting sector brightness; deleting the last light preserves the sector baseline. Lights, movement and wheel edits support undo/redo. Save Level writes a JED-style Section: Lights plus baked vertex intensities to the JKL in the GOB; ordinary play uses those baked values without editor markers. Vertex lighting interpolates between polygon corners, so add cleaves where you need finer lighting detail.

The pause menu's Episode Editor edits episode name/type and up to 64 sequence rows. Click a row to set its line number, CD, level number, LEVEL/CUT type, filename, light/dark force-power grants, Goto A/B, display name and 20 loading-text lines. Add, delete and reorder rows with the sequence buttons. Goto -1 ends the episode; other targets must match a sequence line. Episode type is a numeric bitmask: 1 single player, 2 multiplayer, 8 CTF (combine flags as needed). Apply commits one undoable edit; Cancel discards the draft. Use Save Level afterward to write episode.jk and the associated strings into the GOB. The marked current row receives the edited geometry under its chosen JKL filename; other rows reference level/cutscene files already available to the game. The current level's name/loading text is shared with Level Header. Changes to the episode sequence take effect when the saved episode is loaded.

Weapon 5 opens the Placed COGs editor with middle mouse. Add COG searches scripts in the mounted resources, the original game episode GOBs, and loose cog folders. Select a script on the left; click a parameter on the right. Thing, sector and surface parameters return to the level with only that target type highlighted: aim and press Fire 1 to assign it, or Esc to cancel the pick and reopen the menu. Materials can be picked from surfaces, templates from things, and vectors from aimed surface points. Numeric/resource parameters accept typed values; COG references choose another placed COG. Edit value also allows manual item numbers; Clear value unassigns a reference (-1) or zeros a numeric/vector value. Local script symbols are omitted. Apply commits the draft as one undoable edit, including all assignments. F8 starts/restarts placed scripts for playtesting; editor mode suspends them. Save Level writes the COGS section and bundles each selected script in the GOB. Up to 64 placed scripts and 128 exposed parameters per script are supported.

Multiplayer editing uses the existing game connection. Everyone must use this
updated GeoSith build with the JK game assets. Host an editor episode as a
multiplayer game; joining players do not need the host's episode GOB. The client
creates a private netedit episode to connect, then receives the full current level
before the normal JK join sync begins. This includes geometry, surface attributes,
materials by name, objects and frames, lights, level/episode headers, and placed
COGs with their scripts and parameter assignments.

The host automatically validates every submitted edit and its document revision.
Client edits appear immediately as local previews while the host checks them.
Accepted edits are broadcast to everyone; rejected previews revert immediately
to the last accepted level and then synchronize with the current host version.
Newer drag positions stay visible when an older position is approved. Continuous
drags share one undo transaction. Ctrl+Z and
Ctrl+Y on a client request the shared host history (30 steps); the accepted result
is replicated to everyone. If another player changes the level before a pending
edit can be accepted, the stale request is rejected and the current host version
is restored. Re-grab the item to continue editing. An incoming edit also cancels
an active grab on the host so that an old drag cannot overwrite the new change.

Level transfers use bounded chunks, checksums, acknowledgments and retries;
partial snapshots never replace the running level. Private netedit GOBs are
connection caches and do not overwrite existing episodes. Save Level exports the
current accepted document as a normal playable GOB; clients wait for pending
edits to finish before saving.

Multiplayer connectivity and troubleshooting: see NETWORKING.md. The new network
protocol requires matching GeoSith releases and game Resource archives.

Whole-surface movement includes shared boundary junctions omitted from the selected
face loop. For example, raising a ceiling also raises the wall-cleave endpoints
on its perimeter, preserving existing partitions rather than generating repair
triangles and interior vertices.

Weapon 1 has Select and Clean Up modes in its middle-mouse menu; the mouse
wheel also switches between them. Select keeps
the normal selection, dragging and extrusion behavior. In Clean Up, aim at a
surface and press Fire 1 to simplify its entire sector. Connected coplanar
patches with matching materials, UV mappings, flags and rendering settings merge
when their union is convex and fits the 24-vertex limit. Redundant interior and
collinear vertices disappear. Concave regions and material/settings boundaries
remain separate. Adjoin patches merge only with matching reciprocal patches in
the neighbor, preserving connections. Cleanup is one undoable edit and uses the
normal multiplayer host validation and replication path.

Cleanup also rebuilds dense coplanar solid meshes around preserved openings
and other material regions into simpler convex patches. This can reduce a
fragmented wall to a few quads even when its concave outline cannot form one
convex polygon. Openings, volume, material/UV boundaries, flags and adjoins are
preserved; each proposed merge must retain a closed sector boundary.

Stock-level conversion
----------------------
Opening a level from JK1.GOB or JK1MP.GOB imports it into GeoSith for F8 editing, including multiplayer synchronization. Save Level suggests the map filename with `_edited` appended (for example, `m10_edited`). JK1 and JK1MP are protected save names. Only the current map is saved in the new episode; the original campaign/episode archive is unchanged.

Conversion retains original UV coordinates, baked vertex lighting, sector flags/tint/ambient sounds, object AI settings and paths, and remaps placed COG references to the editor's object/surface order. Imported sectors allow up to 0.001 game units of rounding error; newly created sectors retain the stricter checks. Invalid or over-capacity imports fail without replacing the running geometry.

The multiplayer editor protocol changed for the imported metadata: hosts and clients must use the same updated build. Undo snapshots use compressed populated records to limit memory use on large maps.

GeoSith - Windows x64 redistributable

Getting started
1. Extract this entire folder to a writable location.
   Recommended: Documents\GeoSith. Avoid Program Files and Program Files (x86),
   where Windows normally blocks the editor from writing its catalogs and levels.
2. Install your own copy of Jedi Knight: Dark Forces II. GeoSith does not
   include Jedi Knight game assets.
3. Run "Setup GeoSith.cmd" and enter the folder containing your game's
   Episode and Resource folders. You can also copy your own installation
   into the supplied JK folder. Use a writable game folder: setup generates
   editor catalogs there, and new levels are saved in its Episode folder.
4. Run "Launch GeoSith.cmd". Select Create New Level, then press F8 in game
   to enter the editor. See EDITOR-GUIDE.md for controls and tools.

Setup uses Windows PowerShell and does not require Python, Visual Studio,
or access to the original developer's project. The supplied DLLs belong
beside GeoSith.exe. Keep the licenses and source archives with this package.

The required original data includes JK1.GOB and JK1MP.GOB in Episode,
Res1.gob (or Res1hi.gob) and Res2.gob in Resource, plus your JK_.CD marker.
Do not copy Jedi Knight executable files over GeoSith.exe.

If setup reports Access denied, move the entire GeoSith folder and its JK
subfolder to Documents\GeoSith, then run Setup GeoSith.cmd again. If Windows
offers to replace existing files while copying the game, keep GeoSith's supplied
files. Clear Read-only on generated editor catalogs if necessary. GeoSith does
not need to run as administrator when its folders are writable.

Contents
- GeoSith.exe: the editor integrated with OpenJKDF2.
- OpenAL32.dll: audio runtime.
- GameNetworkingSockets.dll: multiplayer runtime.
- Visual C++ runtime DLLs: app-local Windows x64 runtime dependencies.
- Setup / Launch GeoSith: first-run preparation and launcher.
- licenses: engine and dependency notices.
- source: corresponding project/dependency source snapshots and build notes.

Jedi Knight resources, generated catalogs/templates, saved levels, player
profiles, developer paths, and debug symbols are not part of this package.
They are supplied or generated locally by each user after setup.

Multiplayer setup and troubleshooting: see NETWORKING.md. Both players need the
same GeoSith release and matching game assets. Internet hosting may require UDP
port forwarding or a shared VPN if automatic router mapping is unavailable.

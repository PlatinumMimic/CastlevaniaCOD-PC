# Castlevania: Curse of Darkness - XBE Recomp

An unofficial native PC port of the original Xbox version of Castlevania: Curse of Darkness, statically recompiled
into a 64 bit Windows executable. Rendering runs on Direct3D 12 and sound on XAudio2.

This port was created using the [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) tool.

> [!IMPORTANT]
> This project does not include any game assets or game code. You must provide the files from your own legally
> acquired copy of the game. See [Legal](#legal).

## Required Game Version

Only the USA release is supported. The config is tied to the exact code addresses in its `default.xbe`, and the
built exe refuses any other `default.xbe`. Many copies have the xbe's certificate patched (its title, allowed media
and region, for a hard disk loader or any region's console); the code is the same, and the game puts those fields
back as the release has them and plays that.

| | |
|---|---|
| Game | Castlevania: Curse of Darkness, original Xbox, **USA (NTSC-U)** |
| Title ID | `4B4E002D` (KN-045), version 1, XDK 5849 |
| `default.xbe` size | 7,352,320 bytes |
| `default.xbe` SHA-1 | `423BC19B7CED263570DCE5E7CD5D81954DFBE13C` |
| `default.xbe` MD5 | `92176FDDF071667F596680DF64DED7FE` |

Other regions (PAL, Japan) will not work. The launcher's Game files page tells which of your copies will play, and
reads the xbe inside a disc image too.

## Installation

1. Download the latest release from [Releases](https://github.com/Pazzi7054/CastlevaniaCOD-PC/releases) and extract it
   anywhere.
2. Run `Castlevania.exe`. The `assets` folder opens.
3. Put your game in it: the disc image (`.iso`, a whole disc dump or an xiso), or the extracted disc (`default.xbe`
   with its `data`, `movie` and `sound` folders).
4. Run `Castlevania.exe` again. The first start builds the game code from your copy, which takes a few minutes and
   shows its progress; press **Start** when it is done. After that the game starts straight away.

A release contains no game code. `Castlevania.exe` holds the runtime, and the first start makes `Castlevania.dll`
from your own `default.xbe` with the recompiler, [Clang and LLD](https://llvm.org) in the `compiler` folder. The
Visual C++ runtime comes with it, so a fresh Windows needs nothing else.

The port's programs are not code-signed, so the first start of a download may bring up "Windows protected your PC":
**More info**, then **Run anyway**. On a freshly installed Windows there is nothing to download besides the release and
a graphics driver; see [Requirements](#requirements).

- `CastlevaniaLauncher.exe` has every setting and checks your game files; `Castlevania.exe` skips it.
- The game can run from the extracted files in `assets`, or from an ISO of the disc placed in `assets`.
- Recomp settings can be adjusted in `castlevania.toml` beside the exe. Saves go to `hdd\E\UDATA\4b4e002d`.

## Features

### Launcher
`CastlevaniaLauncher.exe` checks your graphics (whether they have the Direct3D 12 the game draws with, as the game picks
them: the high performance GPU first; `CV_NO_D3D12=1` shows the warning anywhere) and your game files (the USA
`default.xbe` by its hash, or a disc image), adds a disc image to `assets` for you (a hard link on the same drive,
otherwise a copy), and has every setting: display mode, resolution, v-sync, widescreen, render scale, anisotropic
filtering with quick presets, the HD texture pack, FPS counter, pause on focus loss, the settings menu key and the
button layout. Changes are saved as you make them; **Play** starts the game. Saves and logs are a click away. `--page
controls` (or home, display, graphics, game, files) opens it on that page. It looks like the game's menus: navy with fog
and a teal band, page names on orange banners, the chosen option on a green bar, boxes framed in light blue, Palatino
for the game's serif. Its home page has the title logo from your HD texture pack (else the name in gold) and the crest
of its icon.

### Settings Menu
Press **F10** in game for the settings menu, or click both sticks in (**L3 + R3**) on a controller. The key can be
changed in the launcher (Game tab), for keyboards where F10 needs Fn or is taken by another program. Every change
applies at once and is saved to `castlevania.toml`. While the menu is open the game gets no input; Escape, the menu
key or L3 + R3 closes it.

### Display
- **Any Resolution** - Renders at your display's resolution instead of 640x480.
- **Display Modes** - Fullscreen, borderless or windowed, with vsync.
- **Widescreen** - On a 16:9 or wider display the game is shown 16:9 with more of the world at the sides (Hor+),
  without stretching. The HUD, text and movies stay 4:3 in the middle; the backdrops of the menus, the map and the
  save screens reach the sides, as does the dimmed panel behind a message. Turn it off in the menu (Graphics) for
  the original 4:3 picture.
- **No Overscan Streaks** - The rows a TV's overscan used to hide at the top and bottom of the picture no longer
  smear; the frame fills the screen.

### HD Texture Packs
Texture packs made for PCSX2 work as they are: put a pack's replacement pictures in a `textures` folder beside the
game (folders inside it are fine). The Xbox release kept the PS2's textures, so each one is matched by the same
hashes PCSX2 names the pictures by. Pictures load in the background with full mipmaps, scaled to the size set in the
launcher (1024, 2048 by default, or full size). Paletted textures are replaced; region (`-rWxH`) and true colour
pictures are not used yet.

### Graphics
- **Native Direct3D 12 Renderer** - The game's Direct3D 8 calls, fixed function stages and paletted textures are
  drawn natively.
- **Render Scale** - 50% to 200% of the output height; above 100% supersamples.
- **Anisotropic Filtering** - Up to 16x.
- **Shader Cache** - Shaders compiled while playing are kept in `cache\shaders` beside the game, so later sessions
  do not stutter the first time an effect is drawn.

### General
- **FPS Counter** - Optional, in the corner.
- **Pause on Focus Loss** - The game stops while another window is in front (on by default).
- **Lament of Innocence Save** - As the PS2 game did when it found a Lament of Innocence save on the memory card:
  a Moai to pick up at the far end of the room behind the first courtyard, beside the potion, for completing the
  item list (the launcher's or the in-game menu's Game page; `lament_save = true`).

### Controller
Supports Xbox, PlayStation and Switch controllers, with hot plugging and rumble.
- **Button Layout** - Xbox, or PS2: the PS2's button icons (□ △ ○ ×, L1 L2 R1 R2) and its shoulders, L1 resetting
  the camera and R1 guarding, L2 and R2 locking on (the launcher's or the in-game menu's Controls page;
  `button_layout = 1`; in game, from the next start). The PS2 icons come from an HD texture pack made from the PS2
  game (HD Texture Packs, above), and the launcher says whether yours has them; without one the Xbox icons stay.

### Keyboard
The keyboard plays as player 1 while the window is in front:

| Keys | Pad |
|---|---|
| W A S D | left stick (move) |
| arrow keys | right stick (camera) |
| K or Space | A (jump) |
| J | X (attack) |
| L | B |
| I | Y |
| Q / E | left / right trigger |
| Z / C | white / black |
| Enter / Backspace | Start / Back |
| numpad 8 2 4 6 | d-pad |
| F10 (changeable) | settings menu (L3 + R3 on a controller) |

### Miscellaneous
- **Boot Splash** - Place a `splash.png` (or `.bmp`) beside the exe to display a splash screen at launch.
- **Crash Dumps** - Written to a `CrashDumps` folder beside the exe.

## Requirements

### To Play
A release needs nothing installed on a fresh Windows but the graphics driver:

- **Windows** 10 (version 1703 or later) or 11, 64-bit.
- **GPU** with Direct3D 12 (feature level 11_0), and its driver. Direct3D 12 is part of every Windows 10 and 11, but
  works only where the graphics chip has a driver for it: NVIDIA GeForce 600 series (2012) or newer (the 400 and 500
  series got it only in their last drivers), AMD GCN cards, Radeon HD 7700 to 7900 (2012) and later, or Intel's 4th
  generation Core graphics (Haswell, HD 4200 to 4600 and Iris, 2013) or newer. A PC with only older graphics, such as
  the Intel HD 3000 or 4000 of a 2011 or 2012 laptop, runs Windows 10 but not the game, which says "Direct3D 12 could
  not be started". To check: `dxdiag`, the Display tab's **Driver Model** must be WDDM 2.0 or higher (its System tab
  says DirectX 12 on every Windows 10, whatever the graphics).
- **CPU** with SSE4.2 and POPCNT, as the game code is compiled for x86-64-v2: Intel since the first Core i (2008),
  AMD since Bulldozer (2011). The graphics are the stricter limit on old PCs.
- **Disk**: about 0.2 GB for the release, and 0.2 GB more once the first start has built the game code (it wants
  1 GB free to start; its ready page can delete the compiler folder afterwards), besides your game: 3.1 GB as a disc
  image, 2.9 GB extracted.
- **Your copy of the game**, the USA release (see [Required Game Version](#required-game-version)).
- Optional: a controller (Xbox, PlayStation or Switch; the keyboard plays too), and an HD texture pack.

Everything else comes in the zip or with Windows. The zip has the Visual C++ runtime (`msvcp140.dll`,
`vcruntime140.dll`, `vcruntime140_1.dll`) and the recompiler, Clang and LLD the first start builds the game code
with; Direct3D 12, the shader compiler (`d3dcompiler_47.dll`), XAudio2 and the C runtime (UCRT) are part of
Windows 10 and 11. Playing needs no internet connection, the first start neither.

### To Build
On a fresh Windows, install:

1. [Git for Windows](https://git-scm.com/download/win). CMake fetches the libraries below with it too.
2. [Visual Studio](https://visualstudio.microsoft.com/) 2022 or newer (Community is free) with the workload
   **Desktop development with C++** (the MSVC tools and the Windows SDK), and from Individual components
   **C++ Clang tools for Windows** and **C++ CMake tools for Windows** (CMake and Ninja). `build.bat` offers to add
   these two if they are missing.
   Visual Studio's Clang is enough (19.1 builds and plays). [LLVM](https://github.com/llvm/llvm-project/releases) from
   llvm.org (`winget install LLVM.LLVM`) is used instead when it is installed in `Program Files\LLVM` (23.1 tested).

The first build also needs an internet connection: CMake downloads [SDL](https://github.com/libsdl-org/SDL) 3.4.16,
[toml++](https://github.com/marzer/tomlplusplus) 3.4.0, [Dear ImGui](https://github.com/ocornut/imgui) 1.91.0,
[Zydis](https://github.com/zyantific/zydis) 4.1.1 and [xxHash](https://github.com/Cyan4973/xxHash) 0.8.2. The build
takes about 1 GB of disk (`out` and `generated`), besides the game's 2.9 GB in `assets`. Python is not needed to
build or to make a release; an ISO is extracted with PowerShell, which Windows has.

### To Test
Only for the tests in `tools\cvtest` (see [Testing](#testing)):

- [Python](https://www.python.org/downloads/) 3.8 or newer (tested with 3.14), with **Add python.exe to PATH**
  ticked in its installer. The tests use its standard library alone.
- For `--judge` and a step's `"ask"`: the [Codex CLI](https://github.com/openai/codex), signed in. For
  `--judge=jev-latest`: a TypeSafe API key.

## Building

### 1. Install the Tools
See [To Build](#to-build).

### 2. Clone the Repositories
Clone this repository and the SDK beside it, then apply this port's SDK patch:

```
git clone https://github.com/Pazzi7054/CastlevaniaCOD-PC
git clone https://github.com/TGP482/XboxXBERecomp XboxXBE-Recomp
git -C XboxXBE-Recomp checkout 91146ff
git -C XboxXBE-Recomp apply ..\CastlevaniaCOD-PC\sdk-patches\xboxxbe-recomp.patch
```

### 3. Add the Required Game Files
Put your disc image (`.iso`) in the `assets` folder, or the extracted disc: `default.xbe` with the `data`, `movie` and
`sound` folders beside it. With only a disc image there, `build.bat` first extracts its files into `assets`
(`tools\extract_iso.ps1`, a minute or less), as the build reads `default.xbe`.

### 4. Build the Project
Run `build.bat`. The first build recompiles the game and takes a while. `Castlevania.exe` and
`CastlevaniaLauncher.exe` are written to `out\build\release`, with the game code built in. Their icon, made by
`tools\xbe_icon.cpp`, is the game's crest in white on a black disc. It comes from your HD texture pack's 512x512
picture of it (the PS2 menu's, with its ring of runes at the larger sizes), or else from the crest on the Xbox
dashboard, 128x128 in your `default.xbe`. A release has no icon, as it packs nothing of the game.

### 5. Make a Release
`release.bat 1.0.0` writes `out\release\CastlevaniaCOD-PC-1.0.0.zip`: `Castlevania.exe` and the launcher, built in
`out\build\package` (the `package` preset: no icon, and no `Castlevania.dll`, the game code), the `compiler` folder
the exe's first start builds the game code with on the player's PC (without its `work` folder), and the Visual C++
runtime. It checks that nothing of the game went into the package. `tools\cvtest\install.py` then tests the zip as
a player would install it (see Testing).

## Port Notes

Changes this game needed beyond the generated config:

- `config\game.toml`, by hand: `D3DDevice_SetPalette`, `D3DDevice_DrawIndexedVerticesUP` and four DirectSound buffer
  methods (`SetPitch`, `PlayEx`, `SetDopplerFactor`, `SetRolloffFactor`) that no 5849 signature matched; the
  DirectSound ones are named from the image's `.XTLID` table (the SDK now finds `Pause` itself).
- `sdk-patches\xboxxbe-recomp.patch`: host versions of `DrawIndexedVerticesUP` and the four DirectSound methods; a
  fixed-function stage with no texture that reads `D3DTA_TEXTURE` ends the stage cascade, as XDK D3D does (without it
  every single texture quad drew white); `XBE_VERBOSE` lets a release build log info lines; a `widescreen` setting;
  for renderer extensions, `ShaderHooks::Position` (HLSL after the vertex position is set), `Extension::Vertices`
  (each draw's assembled vertices) and `gpu::ReadsConstant`; an extension's replacement is drawn before the SDK's own
  texture pack image; multisampled draws (2X, 4X) on the hardware's smaller grid, which puts Hector's 4X stencil
  shadow volume back in place (found by Neguchads). The SDK's mixer plays every channel of a voice by itself now
  (cutscene audio is a 5.1 stream with the voices in the centre channel).
- Releases are the SDK's own (`release.bat`): the game code is `Castlevania.dll`, which the exe builds on the
  player's PC on its first start (the SDK's `runtime\app\game_code.cpp`, behind a progress window) from their own
  `default.xbe`. A DLL whose stamp does not match the exe and xbe is built again.
- The game's copy: the SDK patch has the exe take a copy whose certificate was patched (title name, allowed media,
  region, ratings) once those are put back as in the build's own xbe, which CMake reads them from
  (`kImageCertificate`, beside `kImageSha1`); the game then plays the restored xbe. A disc image may also be a whole
  disc dump of either layout (the game partition at `0x18300000` on XGD1 discs, `0xFD90000` on XGD2). The launcher
  checks the same way, the xbe inside each disc image included, with the SDK's disc reader.
- `src\common\menu.cpp`: the menu key's state is read every frame instead of waiting for its key messages, which an
  overlay or another program's hotkey can take first; the messages are still swallowed so F10 never puts the window
  in menu mode.
- `src\launcher`: the launcher, Direct3D 11 and Dear ImGui, no game code; it checks `default.xbe`, and the one on
  each disc image, against the SHA-1 the build wrote for `Castlevania.exe`.
- `src\common\texpack.cpp`: PCSX2 texture packs. A P8 texture's indices are put back into GS memory order, 16x16
  (8 bit) or 32x16 (4 bit, indices under 16) blocks in the GS's column layout, and hashed with XXH3-64 block by
  block as PCSX2 does; its palette is hashed as the GS holds it (R G B A, alpha halved). The pair names the picture.
  PNGs load on worker threads through WIC with their PS2 alpha (128 opaque) doubled. The SDK patch adds
  `Extension::Replace` and `Replaced`, `gpu::PaletteIndices`, and the backend draws a replacement with every level.
- `src\game\hooks.cpp`: widescreen culling. The camera (`sub_00012CD0`, a PS2 libvu0 style camera) makes its
  view screen matrices and the clip data objects are culled against; with widescreen on, the clip data's x scale
  (`0x012FC500`, `0x012FC5C0`, `0x012FC640`, `0x012FC660`) is made 3/4, so objects in the wider view are drawn
  instead of popping in at the edges. The matrices the scene is drawn with are untouched.
- `src\common\widescreen.cpp`, a renderer extension:
  - Widescreen. The game projects with a PS2 style view screen matrix (`c68`, or `c85` with the world matrix) and
    maps PS2 screen space to console pixels with `c94`/`c95`. Each draw into the back buffer is squeezed to 3/4 of its
    width around the centre: for draws that use the camera that is a wider field of view, for 2D draws a 4:3 shape.
    Draws over the whole width stay whole when they are untextured (fades), take a few texels across it (a message's
    dimmed panel) or read a render target (the frame pass). Menus, the map and the save screens draw their backdrop
    in pieces; the pieces at the frame's edges have those edges pulled out to the screen's (a vertex on the edge is
    not squeezed), unless the 3D world (1000 vertices of it) or a 4:3 picture over the whole frame came first: the
    title's logo is in pieces at the edges, over its castle. `CV_WIDE_TRACE=<n>` logs every draw into the back buffer
    of one frame in n, where it lands and what was made of it. Only draws into the device's back buffer are squeezed:
    the game composes an area's name card in another 640x480 target and copies it out to a texture, and squeezed
    there the card lost its left end.
  - Overscan. The last pass of each frame draws a copy of the frame back with its rows -16 to 496 over rows 0 to 480
    (PS2 448 lines plus border shrunk into the NTSC frame); the 16 rows past each edge clamped into streaks. The pass
    is drawn that much taller, so the copy fills the frame row for row.

### Testing
- `python -I tools\cvtest\cvtest.py tools\cvtest\scenarios\newgame.json` plays a scenario in a test folder of its own
  (`out\test\<scenario>-<time>`, its own settings and saves) through `CV_PAD_LIVE`, checks the game state after
  every step (`src\common\telemetry.cpp`: map and room, Hector's position, HP, the camera, menus), the log and
  the screen, and writes `report.md` with a screenshot per step. Scenario format: the top of `cvtest.py`.
  `bot.json` has the test bot (`src\common\bot.cpp`) walk Hector to points and fight the knights of room 5.12.
- `python -I tools\cvtest\cvtest.py --record <route> [--save <name>]` starts the game for play (the play folder's
  settings, keyboard and controllers) and records it; once the game is closed the recording becomes
  `tools\cvtest\routes\<route>.json`, a leg per room with the points walked, the jumps and B presses (doors,
  save points), the menus and message boxes as they were answered, and the fights that were won. A scenario step
  `{"route": "<route>"}` plays it back with the bot: `route.json`, and `first-save.json` (a new game to the first
  save room, a save there, the pause menu and the map).
- `--judge` has every screenshot judged by GPT-6 Luna through the Codex CLI (`codex exec`, on its sign-in;
  `tools\cvtest\judge.py`): what the screen shows, whether it rendered right, whether Hector is in it, about 4 s a
  picture. A likely defect, or a scene the game state disagrees with, is a warning in the report.
  `--judge=jev-latest` is quicker for long runs such as sweeps: Jev (TypeSafe, the key in `TYPESAFE_API_KEY` or a
  `.env` file named by `TYPESAFE_ENV_FILE`) screens every shot in about 0.35 s and Luna looks again at those it
  flags (`--review=none` for Jev alone). `tools\cvtest\bakeoff.py` builds the labelled set (shots from test runs,
  rendering defects planted in some) that Jev, GPT-6 Luna and Haiku 5.5 were compared on.
- `perf.json` stands 5 s in the heaviest rooms with the SDK's timings (`XBE_PERF`) and ends its report with them:
  the frame rate, the renderer's milliseconds a frame, the game thread's in D3D calls and waiting on the renderer,
  the draws. A scenario's `"perf": true` times any run; `"shader_cache": "<name>"` keeps its compiled shaders for
  the next run with that name (a test starts cold otherwise).
- A step's `"ask"` puts a question about its screenshot to GPT-6 Luna, with named answers an `expect` can check.
  `buttons.json` asks which controller's icons show at the courtyard door and on the controller settings page;
  `--set button_layout=1` runs it with the PS2 layout (`--set` changes any setting for a run).
- `CV_TEXPACK_MISSES=1` logs each texture the HD pack has no picture for, once, as a pack would name it.
- `widescreen.json` keeps the screens widescreen reaches the sides of: the pause menu, the map, and with a route
  step's `"box_shots": true` each box on the first-save recording (the save point's, and the Memorial Ticket's
  dimmed panel when the first enemy drops it).
- `moai.json` walks a recording (`routes/moai.json`) to the far end of 5.0 and asks whether the Moai floats there and
  what the pickup's notice says: the Moai with `--set lament_save=true`, nothing without it.
- `python -I tools\cvtest\install.py out\release\CastlevaniaCOD-PC-1.0.0.zip <your .iso, or an extracted disc>`
  installs a release as a player would and plays it: the zip extracted into a new folder in TEMP (outside the
  repository, where the game could find this PC's `assets` or `textures`), the game linked into its `assets`, every
  program and DLL in it checked to load from the package or Windows alone (the Visual C++ runtime does not count, a
  fresh Windows has none), then `Castlevania.exe` started in an environment of Windows' own variables only, its
  first start building the game code, and `newgame.json` (or the scenario given) played there. The report is
  `out\test\install-<scenario>-<time>`, the checks and the first start before the steps.
- `tools\run_test.ps1 -Name x -At 10,20 -Env "CV_PAD_SCRIPT=@tools\scripts\newgame.txt"` runs the game, screenshots
  it at the given seconds into `tools\shots`, and closes it. It copies the screen, so keep the game window in front;
  give `CV_PAD_SCRIPT` a full path, the game runs from `out\build\release`.
- `CV_PAD_SCRIPT` presses buttons on a timeline (see `src\common\keyboard.cpp`).
- `XBE_VERBOSE=1` logs info lines to `logs\castlevania.log`; with `XBE_CAPTURE_DIR` set, F3 saves a frame with its
  draw list.

## Known Issues
- Only the first areas have been tested so far.
- Nothing in a release is code-signed (the port's programs, nor the Clang and LLD of the LLVM project). Windows 11's
  Smart App Control, where it is on, may block such programs; that is untested.

## Legal

This is a non-commercial fan project for preservation and interoperability. It is not affiliated with, endorsed by
or sponsored by Konami or Microsoft.

- This repository and its releases contain only original source code, configuration and documentation, the port's
  own runtime, and the LLVM compiler. They contain **no** game assets, game code (original or recompiled), disc
  images, BIOS, keys or Xbox SDK libraries, and none may be added. `assets`, `generated` and all build output are
  excluded by `.gitignore`; `release.bat` refuses a package with game files or objects in it.
- `config\game.toml` lists addresses and names of functions in the game's executable. It contains no code from it.
- The recompiled code is generated on your own PC from your own copy of the game. `Castlevania.dll`, and an
  executable from `build.bat`, are for your personal use only. Do not distribute them, generated code or game files.
- Castlevania and Curse of Darkness are trademarks of Konami Digital Entertainment. Xbox is a trademark of
  Microsoft Corporation. All other trademarks belong to their respective owners and are used only to identify the
  game this port works with.

The source in this repository is released under the [GNU General Public License v2.0](LICENSE). The SDK patch in
`sdk-patches` modifies
[Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) and is under its MIT License, Copyright (c) 2026 TGP482.

## Credits

- [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) by TGP482, the recompiler and runtime this is built on.
- [LLVM](https://llvm.org) for Clang and LLD, which releases bundle to build the game code on the player's PC.
- [xxHash](https://github.com/Cyan4973/xxHash) for hashing textures, and [PCSX2](https://pcsx2.net) for the texture
  pack naming its replacement pictures follow.
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) for the overridable function layout and instruction hooks.
- [Zydis](https://github.com/zyantific/zydis) for decoding x86 instructions, [toml++](https://github.com/marzer/tomlplusplus) for the configs, [Dear ImGui](https://github.com/ocornut/imgui) for the settings overlay and [SDL](https://github.com/libsdl-org/SDL) for the window, input and controllers.
- [xemu](https://github.com/xemu-project/xemu) and [Cxbx-Reloaded](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded) as references for the Xbox's hardware and API behaviour.

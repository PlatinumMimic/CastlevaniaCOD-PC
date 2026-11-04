# Castlevania: Curse of Darkness - XBE Recomp

An unofficial native PC port of the original Xbox version of Castlevania: Curse of Darkness, statically recompiled
into a 64 bit Windows executable. Rendering runs on Direct3D 12 and sound on XAudio2.

This is a personal, non-commercial fan project. It has nothing to do with Konami, Microsoft or any other company: it
is not affiliated with, endorsed or sponsored by them, and it is not an official release. See [Legal](#legal).

> [!IMPORTANT]
> **You need your own copy of the game.** This project does not include, and will not provide, the game: no `.iso`
> of the disc, none of its files and none of its code. The port plays from an `.iso` (or the extracted files) of
> your own USA Xbox disc; see [Required Game Version](#required-game-version).

## Where This Comes From

This repository is a fork of [CastlevaniaCOD-PC](https://github.com/Pazzi7054/CastlevaniaCOD-PC) by Pazzi7054, the
port that took the Xbox game from boot to gameplay on PC, with its launcher, settings menu, widescreen, any
resolution and PCSX2 texture packs. It is built on, and was inspired by:

- [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) by TGP482: the static recompiler and runtime that turn
  the Xbox executable into native x86-64 code, with the game's Direct3D 8 drawn on Direct3D 12 and its DirectSound
  played on XAudio2.
- [Castlevania-COD-Recomp](https://github.com/Neguchads/Castlevania-COD-Recomp) by Neguchads, another port of the same
  game. Its findings are used here, credited where used: why Hector's shadow went missing, and where the game keeps
  the camera, the maps, the rooms and the enemies.
- [PCSX2](https://pcsx2.net), the PS2 emulator, whose names for replacement textures the HD packs use, so packs made
  for the PS2 game work here as they are.

Every project used is listed under [Credits](#credits).

## What This Fork Adds

On top of Pazzi7054's port:

- **Plays from any copy of the USA disc** - an `.iso` (an xiso, or a whole disc dump of either disc layout) or the
  extracted files. Copies whose xbe header was patched for a hard disk loader or another region play too, and the
  launcher tells which of your copies will.
- **Hector's shadow** - drawn by the Xbox with 4X multisampling, it is back under his feet.
- **Widescreen menus** - the pause menu, the map, the save screens and the panels behind messages fill a 16:9
  screen, and an area's name card keeps its left end.
- **HD texture packs** - `.dds` packs as well as `.png`; eight Xbox textures the PS2 packs know under other names are
  found (with Panda_Venom and Stan21's pack, 96.8% of what the game draws gets an HD picture); cut-out pictures keep
  clean edges at every distance; a tool makes HD pictures for the textures a pack lacks; and the launcher shows
  whether you have a pack, with a switch for it.
- **HD movies** - an HD movie pack replaces the story's movies with sharper versions, kept in step with the game's own
  frame by frame, so the voices, skipping and fades work as ever. They fill the screen at their own shape (the game
  squashes them to 94% of their height). See [HD Movies](#hd-movies).
- **No stutter the first time an effect is drawn** - compiled shaders are kept on disk between sessions.
- **Anisotropic filtering without a pack** - it sharpens the game's own textures too.
- **PS2 button layout** - the PS2's button icons (from a PS2 HD pack) and shoulder buttons, as an option.
- **Lament of Innocence bonus** - the Moai the PS2 game gave for a Lament of Innocence save, as an option.
- **Crazy mode and Trevor from the start** - the name codes `@CRAZY` and `@TREVOR` work without beating the game first.
- **Frame generation** - AMD FSR 3 or NVIDIA DLSS frame generation, its files downloaded by the launcher from AMD's
  or NVIDIA's own release (see [Graphics](#graphics)).
- **Launcher** - in the game's own look, with the title logo from your HD pack. It chooses the graphics card and says
  whether it has Direct3D 12, and has the button layout, the bonus and the pack switch.
- **Launcher in seven languages** - English, Deutsch, Español, Français, Italiano, Português (Brasil) and Русский,
  from a dropdown.
- **Program icon** - the game's crest, made from your copy when you build.
- **Newer recompiler** - Xbox XBE Recomp moved up to its commit `91146ff`, this fork's changes kept as a patch.
- **Requirements written out** - what a PC needs to play, down to which graphics can run it.
- **Automated tests** - a runner that plays scenarios and checks the game state, warps to any room and sweeps every
  room; a bot that replays routes recorded from real play and wins the fights on them; per-room timings; an AI judge
  of screenshots; and a test that installs a release as a player would. See [Testing](#testing).

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

1. Download the latest release from [Releases](https://github.com/jmafort/CastlevaniaCOD-PC/releases) and extract it
   anywhere.
2. Run `Castlevania.exe`. The `assets` folder opens.
3. Put your game in it: an `.iso` of your own disc (a whole disc dump or an xiso), or the extracted disc
   (`default.xbe` with its `data`, `movie` and `sound` folders). This project does not provide it.
4. Run `Castlevania.exe` again. The first start builds the game code from your copy, which takes a few minutes and
   shows its progress; press **Start** when it is done. After that the game starts straight away.
5. Optional: an HD texture pack, see [HD Textures](#hd-textures). The game plays as it is without one.

A release contains no game code. `Castlevania.exe` holds the runtime, and the first start makes `Castlevania.dll`
from your own `default.xbe` with the recompiler, [Clang and LLD](https://llvm.org) in the `compiler` folder. The
Visual C++ runtime comes with it, so a fresh Windows needs nothing else.

On a freshly installed Windows there is nothing to download besides the release and a graphics driver; see
[Requirements](#requirements).

- `CastlevaniaLauncher.exe` has every setting and checks your game files; `Castlevania.exe` skips it.
- The game can run from the extracted files in `assets`, or from an ISO of the disc placed in `assets`.
- Recomp settings can be adjusted in `castlevania.toml` beside the exe. Saves go to `hdd\E\UDATA\4b4e002d`.

### Windows or Antivirus Warnings

The port's programs are not code-signed (a certificate costs money every year, and this is a free hobby project), and
a new download has not been run by many people yet. So browsers and Windows may warn about it:

- **The browser** may block the download as "not commonly downloaded" or "suspicious". Open its downloads list
  (Ctrl+J) and keep the file: in Chrome **Keep** or **Download suspicious file**; in Edge **...**, **Keep**,
  **Show more**, then **Keep anyway**.
- **Windows** may say "Windows protected your PC" on the first start: **More info**, then **Run anyway**.

Some antivirus programs are also wary of a program that builds code and then loads it, which is what the first start
does. That is not a virus:

- Every line of the code is in this repository, and a release is made by `release.bat` from it. You can
  [build it yourself](#building) instead of downloading.
- The release page lists each zip's SHA-256 and links its [VirusTotal](https://www.virustotal.com) scan, where dozens
  of antivirus programs checked it. In PowerShell, `Get-FileHash CastlevaniaCOD-PC-<version>.zip` shows the SHA-256
  of your download; the same value means the same file.
- The programs' Properties (Details tab) name the project, its version and this page.

If an antivirus moves a file to quarantine, restore it and allow the folder, and please report it to that antivirus
as a false positive (Microsoft Defender's is the [file submission](https://www.microsoft.com/en-us/wdsi/filesubmission)
page). Download the port only from this repository's Releases page.

## HD Textures

Optional. Without a pack the game draws its own textures, as on the Xbox. The Xbox release kept the PS2 game's
textures, so HD texture packs fans made for the PS2 game in PCSX2 work here as they are. These packs are their
makers' work: they are not part of this project, and are linked here with thanks.

| Pack | Made by | Notes |
|---|---|---|
| [Castlevania - Curse of Darkness (USA) HD Remaster](https://gbatemp.net/threads/castlevania-curse-of-darkness-usa-hd-remaster.619688/) | Panda_Venom and Stan21 | On GBAtemp. The pack this port was tested with: 96.8% of what the game draws gets an HD picture. |
| [VK Curse of Darkness](https://drive.usercontent.google.com/download?id=1rEeBsGLc93pQerT_7j-MwzT-otxhGZl-&authuser=0) | VictorKratos | On Google Drive, a 15 GB `.rar`. |
| [Castlevania PS2 4K Texture Packs](https://buymeacoffee.com/platinummin/castlevania-ps2-4k-texture-packs) | PlatinumMimic (Calm-Consideration41) | Free, on Buy Me a Coffee; Curse of Darkness and Lament of Innocence. |

To add one:

1. Download a pack and extract it.
2. Copy its pictures into a `textures` folder beside `Castlevania.exe`, with the pack's own folders
   (`textures\SLUS-21168\replacements\...` is fine). The launcher's **Open textures folder** makes that folder.
   Keep one pack in it at a time.
3. Open `CastlevaniaLauncher.exe`. Its home page says how many pictures it found, and its switch turns the pack on or
   off. The pack is read when the game starts, so a change applies from the next start.

`.png` and `.dds` pictures are both read. Pictures of a whole texture are used; the pieces of a texture some packs
also have (`-rWxH` in their names) are not, nor pictures of the PS2's own screen buffers. More on how pictures are
matched, and on the textures the PS2 packs lack, under [HD Texture Packs](#hd-texture-packs).

## HD Movies

Optional. Without a pack the game plays its own 640x480 movies. A movie pack is a `movies` folder beside
`Castlevania.exe` with HD versions of the game's 27 movies (`movie\*.xmv` on the disc), a `movies.toml` that lists
them, and a `.sig` file for each: the original's frames as small thumbnails, which the game's frames are matched to.
A movie the pack lacks plays as the game has it.

To add one, extract it so that `movies\movies.toml` is beside `Castlevania.exe`. The launcher's View page says whether
it found one, and has the switch (**HD movies**) and the size (**Movie size**): **Fit**, the picture as large as the
screen shows it whole, without the black bars (on 16:9 a thin band at each side); **Fill**, the whole screen, a little
of the picture's top and bottom cut off; **Original**, the game's own 4:3 frame. The in-game menu (F10) has both too.

The videos are H.264 (any Windows plays them) or HEVC (Windows needs Microsoft's HEVC extension), decoded on the
graphics card. To make a pack from your own HD versions (each the picture without its black bars, the original's
frames one for one, 29.97 fps, named after the movie: `st01_00.mp4`):

```
python tools\movies\make_movie_pack.py <ffmpeg.exe> <the game's movie folder> <your videos folder> movies --encode
```

`--encode` makes H.264 copies with NVIDIA's encoder (`--cq` sets the quality, 22 by default); `--in-place` leaves
the videos where they are and names them from `movies.toml` instead.

## Features

### Launcher
`CastlevaniaLauncher.exe` checks your graphics (whether they have the Direct3D 12 the game draws with, as the game picks
them: the high performance GPU first; `CV_NO_D3D12=1` shows the warning anywhere) and your game files (the USA
`default.xbe` by its hash, or a disc image), adds a disc image to `assets` for you (a hard link on the same drive,
otherwise a copy), and has every setting: the graphics card (Automatic, the fastest, or any with Direct3D 12: on a
laptop the one in the processor saves battery), display mode, resolution, v-sync, widescreen, render scale, anisotropic
filtering with quick presets, the HD texture pack, FPS counter, pause on focus loss, the settings menu key and the
button layout. Changes are saved as you make them; **Play** starts the game. With the game running, a change made in
the launcher reaches it within a second: HDR and its brightness, render scale, filtering, widescreen, v-sync, and the
game's options (the game looks at `castlevania.toml` twice a second and applies the settings that changed). The
texture pack switch, the size pictures load at and the button layout still take effect from the next start. Saves and logs are a click away. `--page
controls` (or home, display, graphics, game, files) opens it on that page. It looks like the game's menus: navy with fog
and a teal band, page names on orange banners, the chosen option on a green bar, boxes framed in light blue, Palatino
for the game's serif. Its home page has the title logo from your HD texture pack (else the name in gold), the crest
of its icon, and under **Play** the pack's row: how many pictures the `textures` folder has, with the switch for them,
or a button that opens the folder when it has none. The files are looked at again whenever the launcher comes back to
the front, so a pack copied in meanwhile shows.

Its window can be resized, maximized or made full screen (**F11**, **Alt+Enter**, or the button beside the language),
and everything in it scales to the window's size, so all of it shows at any size.

The launcher speaks English (the default), German, Spanish, French, Italian, Brazilian Portuguese and Russian: the
dropdown with the globe at the bottom of its sidebar switches it at once (`launcher_language` in `castlevania.toml`:
`en`, `de`, `es`, `fr`, `it`, `pt-BR` or `ru`; **Reset to defaults** keeps it). The game itself and its in-game
settings menu stay in English. Every phrase is in `src/launcher/words.h`, a row per English phrase with its
translations; a phrase the launcher shows without a row there does not compile, nor does a row that lacks a language.

### Settings Menu
Press **F10** in game for the settings menu, or click both sticks in (**L3 + R3**) on a controller. The key can be
changed in the launcher (Game tab), for keyboards where F10 needs Fn or is taken by another program. Every change
applies at once and is saved to `castlevania.toml`. While the menu is open the game gets no input; Escape, the menu
key or L3 + R3 closes it.

### Display
- **Any Resolution** - Renders at your display's resolution instead of 640x480.
- **Display Modes** - Fullscreen, borderless or windowed, with vsync.
- **Widescreen** - On a 16:9 or wider display the game is shown 16:9 with more of the world at the sides (Hor+),
  without stretching. The HUD and text stay 4:3 in the middle, movies as **Movie size** says; the backdrops of the menus, the map and the
  save screens reach the sides, as does the dimmed panel behind a message. Every page of the pause menu does, not only the first one shoulder buttons
  page away from. The Konami logo and the title's misty castle show more of their own picture at the sides, without
  stretching. Turn it off in the menu (Graphics) for the original 4:3 picture. For a 16:10 screen choose 16:10
  (`widescreen_aspect = 1610`; 169 is 16:9): the same, shown 16:10. 1920x1200 is in the resolution lists even
  where the monitor has no such mode, for a 16:10 window.
- **HUD at the Screen Edges** - In widescreen the health bar, its numbers, the icon at the bottom left and the
  message box at the bottom right sit in the screen's corners, as they do on a 4:3 screen, instead of at the corners
  of the 4:3 middle (on by default; `hud_edges = false` keeps them in the middle). An area's name card, which comes up
  at the right as Hector enters a new area, goes to the screen's right edge with them, all of it as one: the crest,
  the name, the line, and the red flame that burns the card away. Menus, the map and story
  scenes stay as they are.
- **Movies Without Bars** - The story's movies drawn as large as the screen shows their picture whole, at its own
  shape, rather than squashed into the 4:3 frame between black bars (`movie_view`: 1 Fit, 2 Fill, 0 Original), from an
  [HD movie pack](#hd-movies) when there is one (`hd_movies`).
- **No Overscan Streaks** - The rows a TV's overscan used to hide at the top and bottom of the picture no longer
  smear; the frame fills the screen.
- **HDR** - On a display with HDR on in Windows (Settings > System > Display > Use HDR), the game sends an HDR picture
  (scRGB): the darks and midtones as the game shows them, at a scene brightness you choose (200 nits by default), and
  the brightest parts (fire, magic, light, the sky's glow) rising smoothly from there to the display's peak. The
  settings menu is drawn apart at the scene brightness, so it never glares; the game's own HUD is part of its picture
  and its white text rises with the highlights. **Peak Brightness** is the display's own unless you set one (Windows
  reports it; `hdr_peak_nits`, 0 the display's), and **Scene Brightness** sets how bright the rest is
  (`hdr_scene_nits`); both change the picture at once from the in-game menu's Display page. Off (`hdr = false`), or
  on an SDR display, the picture is the usual one. With HDR on in the game, Windows' Auto HDR and NVIDIA's RTX HDR
  stand aside, as they do for any game with HDR of its own.

### HD Texture Packs
Optional: without a pack the game draws its own textures, as on the Xbox. Texture packs made for PCSX2 work as they are:
put a pack's replacement pictures (`.png` or `.dds`) in a `textures` folder beside the game (folders inside it are
fine). The launcher's home page says whether that folder has a pack and how many pictures, with a switch to turn it on
or off before you press Play (also on its Graphics page, with the size pictures load at; in `castlevania.toml`,
`texture_pack = false` turns it off). The Xbox release kept the PS2's textures, so each one is matched by the same
hashes PCSX2 names the pictures by. Pictures load in the background with full mipmaps, scaled to the size set in the
launcher (1024, 2048 by default, or full size). Paletted textures are replaced; region (`-rWxH`) and true colour
pictures are not used yet. With Panda_Venom and Stan21's pack, 96% of the textures the game draws have a picture (1,926
of the 2,000 met in a sweep of every room, 96.8% of their area). Of the rest, some are not in the pack (two round stone
doors, a few effects) and some the Xbox changed (the save screens say HARD DISK where the PS2's say MEMORY CARD; the
HUD's icons have NEW! labels and the d-pad of its own), so the PS2's picture would be wrong for them.
`tools\texpack\upscale.py` makes pictures for those from the game's own textures (see Testing): 48 pictures for the 24
textures, which put in `textures` cover the rest of that sweep. The pack is read when the game starts, so the switch
takes effect from the next start; the in-game menu has none.

**Material maps.** A pack can also have a `materials` folder (beside `replacements`) with a `manifest.json` in the
`pcsx2-material-replacements-v1` format and a normal map and an ORM map (occlusion, roughness, metalness) for each
material it lists, named by the same hashes as the pictures. The game loads them with the pictures and shades each
picture's colour with them: the normal map's relief catches a fixed light (each material's `light_direction`) against
the surface's own facing, the occlusion darkens crevices, and rough and metal parts get the shine their maps give,
each as strongly as the manifest's `normal_strength`, `response_strength`, `specular_strength`, `ao_strength`,
`roughness_bias` and `metalness_scale` say. Flat parts of a surface keep their colour; the shading only adds relief.
The surface's facing comes from its depth in the view, so walls, floors and ceilings each take the light their own
way. The launcher's Graphics page has a switch for them (`texture_pack_materials = false`), from the next start.

### Graphics
- **Native Direct3D 12 Renderer** - The game's Direct3D 8 calls, fixed function stages and paletted textures are
  drawn natively.
- **Render Scale** - 50% to 200% of the output height; above 100% supersamples.
- **Anisotropic Filtering** - Up to 16x, on the game's own textures too: they have no mip levels, so a floor or
  wall seen at an angle shimmers in the distance without it; with the HD pack, whose pictures have mip levels,
  it keeps them sharp there instead.
- **Ambient Occlusion** - Soft shadow where surfaces meet: in corners, between stones, under ledges and where feet
  touch the floor (`ambient_occlusion`: 0 off, 1 low, 2 high). It is worked out from the 3D's depth before the
  anti-aliasing smooths it, so it needs anti-aliasing on.
- **Ray Tracing** - On a graphics card with DirectX ray tracing (DXR 1.1: NVIDIA GeForce RTX, AMD Radeon RX 6000 or
  newer, Intel Arc), rays through the room itself (`ray_tracing`: 0 off, 1 ambient occlusion, 2 reflections, 3 both;
  launcher Graphics page or the in-game menu; anti-aliasing on). Each frame, the vertices the game's own draws put on
  the screen are gathered into an acceleration structure (the world as drawn, characters posed as they are), with
  each draw's picture and texture pack material. Cut-outs (railings, leaves, grates) are seen through where their
  pictures are.
  - *Ambient occlusion* is traced instead of guessed from the screen: each pixel sends rays over its surface, so what
    is off screen or behind something still shades, and an edge's shadow does not end where the screen's view of it
    does.
  - *Reflections*: a surface with a texture pack's material maps reflects what its mirror direction meets, that
    draw's picture lit as the game lit it, blurred as its roughness spreads the rays. The HD pack's maps are rough all
    over (about 0.8, no metal), too rough to reflect anything, so the port gives the pack's iron (railings, gates)
    a metal's gloss and its dressed stone a worn polish that shows at a low angle; a manifest's `reflect_roughness`
    and `reflect_metalness` set a material's own.

  Both together cost about 1.2 ms a frame at 1080p on a GeForce RTX 5080 (the screen space ambient occlusion 0.4 ms).
  The shaders are compiled to DXIL by the Windows SDK's `dxc` when the port is built; a build without it, or a card
  without DXR 1.1, keeps the screen space ambient occlusion.
- **Field of View** - 100% (the game's, which each room's camera sets) to 200%, a wider view as the number goes up;
  the in-game menu shows the view's angle as you move it. It holds through a room change's fade-in; story scenes
  keep the game's framing (`fov = 130`).
- **Draw Distance and Fog** - How far the world is drawn (the game's, 1.5x, 2x or 4x) and how far out its fog and
  darkness start (the game's, 1.5x, 2x, 4x, or none). **Far View** sets both to 4x (`draw_distance = 400`,
  `fog_distance = 400`). Rooms the game fogs heavily open up most; a room's far scenery can end where its fog used to
  hide it. Some skies are painted cards the game placed past a room's far plane, in its fog (the Abandoned Castle's
  rampart has a moon 45,000 units out, its fog ending at 39,000): pushed out, they showed their edges. That sky's
  textures keep the room's own fog, so it looks as the game shows it, while the rest of the room is drawn as far as
  set.
- **Shader Cache** - Shaders compiled while playing are kept in `cache\shaders` beside the game, so later sessions
  do not stutter the first time an effect is drawn.
- **Frame Generation** - A frame made between each two the game draws, for twice the frame rate on a 120 Hz or
  faster screen (`frame_generation`: 0 off, 1 AMD FSR 3, 2 NVIDIA DLSS; launcher Graphics page or the in-game menu,
  from the next start). The game's logic runs one step a frame, so it cannot simply run faster: the frames in between
  are made by AMD's or NVIDIA's frame generation from the frame, its depth, its motion and the frame as it was before
  its HUD (so the HUD and menus are not warped). AMD FSR 3 runs on any Direct3D 12 card; NVIDIA DLSS needs a GeForce
  RTX 40 or 50. Both work in HDR: DLSS takes no scRGB, so while it runs HDR goes out as HDR10 (10 bit, PQ, BT.2020),
  the same picture. Generation pauses while the in-game menu is open; the FPS counter shows the frames reaching the
  screen and the game's own beside them, as "120 FPS (game 60)". Neither vendor's files come with the
  game: the launcher's **Download** button fetches them from AMD's or NVIDIA's own GitHub release into the game's
  folder (AMD: `amd_fidelityfx_loader_dx12.dll` and `amd_fidelityfx_framegeneration_dx12.dll` from FidelityFX SDK
  v2.3.0; NVIDIA: the DLSS-G files of Streamline v2.14.1 into `streamline\`), with their licences in `licences\`;
  `CastlevaniaLauncher.exe --download-framegen 1` (or 2) does the same without the window. Without them the setting
  does nothing and the log says what is missing. The motion is the camera's: the renderer finds the same meshes of
  the level in two frames (by what they draw, as the game streams them into new buffers each frame), takes the
  camera's move from their matrices, and turns the depth buffer into motion vectors with it. Characters and effects
  that move on their own get the camera's motion; the vendors' own optical flow covers much of the rest.

### General
- **FPS Counter** - Optional, in the corner.
- **Pause on Focus Loss** - The game stops while another window is in front (on by default).
- **Lament of Innocence Save** - As the PS2 game did when it found a Lament of Innocence save on the memory card:
  a Moai to pick up at the far end of the room behind the first courtyard, beside the potion, for completing the
  item list (the launcher's or the in-game menu's Game page; `lament_save = true`).
- **Crazy Mode and Trevor Mode** - Name a new game `@CRAZY` for Crazy mode (harder enemies) or `@TREVOR` to play as
  Trevor Belmont, with his whip and sub-weapons. The game takes these names only once it has been beaten; here they
  work from the start. The `@` is on the last row of the name screen's letters. A beaten game is still not recorded
  in your save.

### Controller
Supports Xbox, PlayStation and Switch controllers, with hot plugging and rumble.
- **Button Layout** - Xbox, or PS2: the PS2's button icons (□ △ ○ ×, L1 L2 R1 R2) and its shoulders, L1 resetting
  the camera and R1 guarding, L2 and R2 locking on (the launcher's or the in-game menu's Controls page;
  `button_layout = 1`; in game, from the next start). The PS2 icons come from an HD texture pack made from the PS2
  game (HD Texture Packs, above), and the launcher says whether yours has them; without one the Xbox icons stay.
- **Stick Response** - PS2 (the default), or Xbox. The Xbox release cuts each stick axis to 0 by itself under a
  third of its throw, before the game makes the stick a direction, so near up, down, left or right Hector walks
  straight along them (within 20 degrees at full tilt, 42 at half), and a walk starts at a third of his speed. The
  PS2 setting lets the axes through as the PS2 did, and the game's own round dead zone counts: he walks any way the
  stick points, from a slow walk up (the in-game menu's Controls page; `ps2_stick = false` for the Xbox's).
- **Invert Camera** - Left / right and up / down apart: the right stick (and the arrow keys) move the camera the other
  way (`invert_camera_x`, `invert_camera_y`).
- **Rumble** - As on the PS2: the game drives a PS2 controller's two motors, the large one at 256 speeds and the
  small one on or off. The Xbox release made both of the controller's motors one value from either, so the small
  motor's "on" (1 of 255) that the game sends right after the large one's speed left nearly nothing; on the console
  the large one's still got out first, as its requests were slower. Here the large motor drives the left (heavy)
  motor and the small one the right, and the controller gets the motors as each frame leaves them.

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
git clone https://github.com/jmafort/CastlevaniaCOD-PC
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
  texture pack image; anisotropic filtering forced on textures without mip levels too (it was only forced where a
  texture had them, which no texture of this game does); multisampled draws (2X, 4X) on the hardware's smaller
  grid, which puts Hector's 4X stencil
  shadow volume back in place (found by Neguchads). The SDK's mixer plays every channel of a voice by itself now
  (cutscene audio is a 5.1 stream with the voices in the centre channel). `kernel::g_file_opened`, a game's hook on
  each file the title opens; a replacement uploaded into the texture of the one before it when it has its size. Ray
  tracing (`runtime\gpu\raytrace.h`, `rt_*.hlsl`): the world's draws captured by their vertex shaders into an
  acceleration structure each frame, ambient occlusion and reflections traced through it, and a material's
  `gpu::Material::gloss` for reflections. `XBE_MUTE` and `XBE_BACKGROUND` keep a test's game silent and behind other
  windows; `XBE_D3D12_DRED` logs where the GPU stopped when the device is lost, `XBE_D3D12_DEBUG` the debug layer's
  errors.
- Releases are the SDK's own (`release.bat`): the game code is `Castlevania.dll`, which the exe builds on the
  player's PC on its first start (the SDK's `runtime\app\game_code.cpp`, behind a progress window) from their own
  `default.xbe`. A DLL whose stamp does not match the exe and xbe is built again.
- The game's copy: the SDK patch has the exe take a copy whose certificate was patched (title name, allowed media,
  region, ratings) once those are put back as in the build's own xbe, which CMake reads them from
  (`kImageCertificate`, beside `kImageSha1`); the game then plays the restored xbe. A disc image may also be a whole
  disc dump of either layout (the game partition at `0x18300000` on XGD1 discs, `0xFD90000` on XGD2). The launcher
  checks the same way, the xbe inside each disc image included, with the SDK's disc reader.
- The graphics card: the SDK patch adds a `gpu` setting, a card's name as Windows has it
  (`gpu = "Intel(R) Iris(R) Xe Graphics"`). The game draws with that one if it has Direct3D 12, else, as with none
  named, with the fastest (`EnumAdapterByGpuPreference`), and the log says which. The launcher lists the cards that
  can, the fastest first.
- `src\common\menu.cpp`: the menu key's state is read every frame instead of waiting for its key messages, which an
  overlay or another program's hotkey can take first; the messages are still swallowed so F10 never puts the window
  in menu mode.
- `src\launcher`: the launcher, Direct3D 11 and Dear ImGui, no game code; it checks `default.xbe`, and the one on
  each disc image, against the SHA-1 the build wrote for `Castlevania.exe`.
- `src\common\texpack.cpp`: PCSX2 texture packs. A P8 texture's indices are put back into GS memory order, 16x16
  (8 bit) or 32x16 (4 bit, indices under 16) blocks in the GS's column layout, and hashed with XXH3-64 block by
  block as PCSX2 does; its palette is hashed as the GS holds it (R G B A, alpha halved). The pair names the picture.
  PNGs load on worker threads through WIC with their PS2 alpha (128 opaque) doubled, halved to the size set and
  each level below made with their colour weighted by alpha (the colour under a PS2 picture's transparent pixels
  is often a key colour, a green, which an even mean runs into the edges of what is drawn). The SDK patch adds
  `Extension::Replace` and `Replaced`, `gpu::PaletteIndices`, and the backend draws a replacement with every level.
  Eight Xbox textures are found under the PS2's names (`kAliases`): on the PS2 each used part of its texture, the
  rest holding whatever else was in GS memory, which PCSX2 hashed with it, where the Xbox has that part blank. Their
  used parts are the same pictures, found by palette and by content against the pack, and each was looked at.
- `src\game\hooks.cpp`: widescreen culling. The camera (`sub_00012CD0`, a PS2 libvu0 style camera) makes its
  view screen matrices and the clip data objects are culled against; with widescreen on, the clip data's x scale
  (`0x012FC500`, `0x012FC5C0`, `0x012FC640`, `0x012FC660`) is made 3/4, so objects in the wider view are drawn
  instead of popping in at the edges. The matrices the scene is drawn with are untouched.
- `src\common\movies.cpp`: HD movies. The game decodes its XMV movies itself, each frame into a 640x480 linear
  A8R8G8B8 texture it draws over the frame as one quad, reading its rows -17 to 497 over the 480 (a PS2 TV's
  overscan), so the picture is squashed to 94% of its height. The SDK patch's `kernel::g_file_opened` names the movie
  as the title opens `movie\stXX_YY.xmv`. Each frame uploaded is matched to the original's frames by a 16x12 grey
  thumbnail (40x40 blocks' means, made zero mean and unit length, correlated from a frame back to a second and a half
  on, else across the movie), and its replacement (`Extension::Replace`) is that frame of the HD video, decoded by
  Media Foundation on the graphics card a few frames ahead and placed in the middle of a frame of the game's shape;
  the SDK patch uploads each into the texture of the one before it. `widescreen.cpp` scales the quad about the
  screen's middle so the picture fills the screen at its own shape. The log ends each movie with how many of its
  frames came from the HD video.
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
    there the card lost its left end. Drawn to the frame from that target, its crest, title and line come as draws
    apart that do not touch: they go to a side as one, the side the whole card of the frame before lies on, or the
    crest stayed in the 4:3 middle as the title went to the edge.
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
- `CV_TEXPACK_MISSES=1` logs each texture the HD pack has no picture for, once, as a pack would name it; set to a
  folder instead, it also saves each there as a PNG under that name, with the PS2's alpha as PCSX2 dumps it, so an
  upscaled copy in `textures` replaces it. `CV_TEXPACK_CENSUS=1` logs every texture the game uploads, once, with
  its format, size and what the pack made of it (a picture, none, or not looked for: the pack has P8 pictures only).
  `CV_TEXTURES=<folder>;<folder>` reads pictures from those folders instead of `textures`, a picture in an earlier
  one in place of one with the same name in a later one: pictures tried out beside a pack without changing it.
- `census.json` sweeps every room of every map (as `sweep.json`) with `CV_TEXPACK_CENSUS=<folder>`: `census.jsonl`
  there lists every paletted texture the game drew, by the name a pack's picture of it has, with the pack's picture,
  whether it has a material, how it was drawn (on the 3D world by a plain vertex program or a skinned one, the
  characters', or in 2D, and adding to the target or not), the maps it was seen on and its first room; `originals\` has the game's
  own texture of each. `tools\texpack\make_materials.py` makes normal and ORM maps from that list for the textures
  drawn on the world, not added on (glows), with a picture and no material: DeepBump's normals from each picture (its green turned
  to the hand-made maps' direction and made as strong as they are on average), occlusion from the height those give,
  roughness about the hand-made maps' 0.8, no metal, and a manifest beside them (`generated\materials`), each at the
  hand-made maps' parameters for mixed props. Its `census-verdicts.csv` says what became of every texture.
- `hdr.json` plays with HDR output on (`XBE_HDR_STATS` logs the frame's brightness, `XBE_HDR_DUMP=<folder>` with the
  live entry `set=hdr_dump=1` writes a frame as half floats), turns it off and on, and moves the peak and scene
  brightness; `hdr-perf.json` times a frame with it on and off at 4K. `materials.json` shoots the first courtyard,
  the drawbridge and the rampart, for the same shots with `CV_MATERIALS=0`; `genmaps.json` the rooms with the most
  generated maps (2.1, 9.14). `namecard.json` warps between areas.
- `tools\texpack\upscale.py make` makes a picture for each texture `CV_TEXPACK_MISSES` saved: upscaled 4x, the
  pack's scale, by an AI model (RealESRGAN_x4plus; Python with PyTorch, its docstring has the setup). Where the pack
  has the PS2's picture of a texture the Xbox changed in part, as for the save screens' HARD DISK and the HUD's
  icons, it keeps that picture and takes from the upscale only what the Xbox changed: the parts that differ
  throughout from the pack's picture shrunk (not only along edges, as wherever the pack's art was redrawn), in most of
  the texture's colourings; and where in the result a strong colour lies over a texel not of it, the texture's own
  colour, as the pack's upscale of a sheet of icons runs each into its neighbours: the line the HUD draws under
  "Player" took the pure green of the letters above it in the colouring it is drawn with (found by pictures of one
  colour each, in `CV_TEXTURES`, telling which colouring each part of the HUD is drawn with: the frame and that line
  223ff5, "Player" 2c928c, the HP bar 66d55d). Cut-out shapes and text are upscaled with their colour weighted by
  alpha, so the key colour under their transparent texels (often a green) does not run into their edges; soft
  effects take half of the model's colour and half of a smooth resize, the model giving smoke a texture it does not
  have. A pack made for PCSX2 also has region pictures (`<texture>-<palette>-r<w>x<h>-<TEX0>`: the part of a texture a
  PS2 draw clamped to, hashed alone), which the game cannot find, hashing its textures whole: the pack's HUD pieces
  ("Player", HP, the weapon emblems, the digits) are such. Each in the colours of a texture it makes goes where it
  fits in the upscale (its picture shrunk to the texture's scale against every place, kept where close and in few
  places at most), but where the Xbox changed the texture. `CV_UPSCALE_CPU=1` keeps the model off the GPU, as while
  a game has it. `upscale.py calibrate` measures
  models against the pack's own pictures of the same textures (`kAliases`' and those): of
  RealESRGAN_x4plus, UltraSharpV2, GTAV textures (RGT, and its dither model) and GameAI 2.0, RealESRGAN_x4plus came
  closest by far (31.2 dB on average, 45 dB on one, the others 26.7 to 28.7), the pack likely made with it, so its
  pictures look like the pack's. They are the game's art, as a pack is: they stay out of this repository.
- `title.json` shoots the screens before the title (the Konami logo among them) and the title with its menu, for how
  they fill a 16:9 screen; `CV_WIDE_TRACE=<n>` logs every draw of one frame in n, with what widescreen did to it.
  `pausepages.json` starts from the saves in `out\test\saves\user` (copy a save with an Innocent Devil there: the
  pause menu's pages cycle only with one) and shoots each page. `lights.json` pans the camera in a room lit by the
  purple lamps, a shot every few frames, for a glow that trails or flickers as the view moves.
- `framegen.json` plays a room with the camera turning and frame generation on (`--set frame_generation=2` for
  DLSS; the vendors' files from the build folder or `CV_FRAMEGEN_DIR`): the log says when it generates and why not
  (`XBE_FG_TRACE=1`), `XBE_FG_SHOW_MOTION=1` draws the motion vectors over the frame (grey still, red to the right,
  green down) and `XBE_FG_DEBUG=1` AMD's tear lines. `framegen-perf.json` times the game's own frame rate with it,
  which has to stay at 60. `capture.json` saves one frame's draws and shaders (`XBE_CAPTURE_DIR`, the live entry
  `capture`).
- `widescreen.json` keeps the screens widescreen reaches the sides of: the pause menu, the map, and with a route
  step's `"box_shots": true` each box on the first-save recording (the save point's, and the Memorial Ticket's
  dimmed panel when the first enemy drops it).
- `filters.json` checks that render scale and anisotropic filtering change the picture, with the HD pack and without
  (`--set texture_pack=false`). Hector stands on the drawbridge while each is changed live, as the in-game menu
  changes it (the live entry `set=<setting>=<value>`; one read only at the start is an error there), and a step's
  `"measure"` takes the edge strength of parts of the view (`screen.edges.<name>`; `shots["<shot>"]` has an earlier
  step's screen). With the pack 50% halves it, 200% adds two fifths and filtering a half or more; without it 50%
  takes nearly half, while 200% and filtering take a tenth to a third away, the shimmer of textures without mip
  levels smoothed. Two shots with the same settings are the same.
- `moai.json` walks a recording (`routes/moai.json`) to the far end of 5.0 and asks whether the Moai floats there and
  what the pickup's notice says: the Moai with `--set lament_save=true`, nothing without it.
- `trevor.json` and `crazy.json` type `@TREVOR` and `@CRAZY` on the name screen of a game never beaten and play into
  the first courtyard: the game's mode (`mode`, 2 and 1, with the save's `name`) and Trevor's hearts and sub-weapons
  in the shots. `newgame.json` checks that a plain name gives mode 0.
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

This is a personal, non-commercial fan project, made for preservation and interoperability. It has nothing to do
with Konami Digital Entertainment, Microsoft or any other company: it is not affiliated with, endorsed by or
sponsored by them, and it is not an official product. Nothing here is sold.

- This repository and its releases contain only original source code, configuration and documentation, the port's
  own runtime, and the LLVM compiler. They contain **no** game assets, game code (original or recompiled), disc
  images, BIOS, keys or Xbox SDK libraries, and none may be added. `assets`, `generated` and all build output are
  excluded by `.gitignore`; `release.bat` refuses a package with game files or objects in it.
- `config\game.toml` lists addresses and names of functions in the game's executable. It contains no code from it.
- You need your own copy of the game, the USA Xbox disc, as an `.iso` or its extracted files. This project does not
  provide one, nor tell where to get one.
- The HD texture packs linked under [HD Textures](#hd-textures) are their makers' own work, made for the PS2 game.
  They are not part of this repository or its releases, which neither host nor include any of their pictures.
- The recompiled code is generated on your own PC from your own copy of the game. `Castlevania.dll`, and an
  executable from `build.bat`, are for your personal use only. Do not distribute them, generated code or game files.
- Castlevania and Curse of Darkness are trademarks of Konami Digital Entertainment. Xbox is a trademark of
  Microsoft Corporation. All other trademarks belong to their respective owners and are used only to identify the
  game this port works with.

The source in this repository is released under the [GNU General Public License v2.0](LICENSE). The SDK patch in
`sdk-patches` modifies
[Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) and is under its MIT License, Copyright (c) 2026 TGP482.

## Credits

- [CastlevaniaCOD-PC](https://github.com/Pazzi7054/CastlevaniaCOD-PC) by Pazzi7054, the port this repository is a
  fork of.
- [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) by TGP482, the recompiler and runtime this is built on.
- [Castlevania-COD-Recomp](https://github.com/Neguchads/Castlevania-COD-Recomp) by Neguchads, for its findings in the
  game: Hector's shadow on the 4X multisample grid, and the addresses of the camera, maps, rooms and enemies.
- The HD texture packs' makers: Panda_Venom and Stan21
  ([HD Remaster](https://gbatemp.net/threads/castlevania-curse-of-darkness-usa-hd-remaster.619688/)), VictorKratos
  ([VK Curse of Darkness](https://drive.usercontent.google.com/download?id=1rEeBsGLc93pQerT_7j-MwzT-otxhGZl-&authuser=0))
  and PlatinumMimic, Calm-Consideration41
  ([4K Texture Packs](https://buymeacoffee.com/platinummin/castlevania-ps2-4k-texture-packs)).
- [LLVM](https://llvm.org) for Clang and LLD, which releases bundle to build the game code on the player's PC.
- [xxHash](https://github.com/Cyan4973/xxHash) for hashing textures, and [PCSX2](https://pcsx2.net) for the texture
  pack naming its replacement pictures follow.
- [Real-ESRGAN](https://github.com/xinntao/Real-ESRGAN), the model `tools\texpack\upscale.py` makes HD pictures with
  for the textures the packs lack.
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) for the overridable function layout and instruction hooks.
- [Zydis](https://github.com/zyantific/zydis) for decoding x86 instructions, [toml++](https://github.com/marzer/tomlplusplus) for the configs, [Dear ImGui](https://github.com/ocornut/imgui) for the settings overlay and [SDL](https://github.com/libsdl-org/SDL) for the window, input and controllers.
- [xemu](https://github.com/xemu-project/xemu) and [Cxbx-Reloaded](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded) as references for the Xbox's hardware and API behaviour.

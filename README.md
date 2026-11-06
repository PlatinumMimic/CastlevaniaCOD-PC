# Castlevania: Curse of Darkness - XBE Recomp

[![release](https://img.shields.io/github/v/release/jmafort/CastlevaniaCOD-PC?label=release&color=orange)](https://github.com/jmafort/CastlevaniaCOD-PC/releases/latest)
[![downloads](https://img.shields.io/github/downloads/jmafort/CastlevaniaCOD-PC/total?label=downloads&color=brightgreen)](https://github.com/jmafort/CastlevaniaCOD-PC/releases)
[![license](https://img.shields.io/github/license/jmafort/CastlevaniaCOD-PC?label=license&color=green)](LICENSE)
[![Stars](https://img.shields.io/github/stars/jmafort/CastlevaniaCOD-PC?style=social)](https://github.com/jmafort/CastlevaniaCOD-PC/stargazers)

![Castlevania: Curse of Darkness, an unofficial native PC port for Windows and Linux](docs/images/cover.png)

An unofficial native PC port of the original Xbox version of Castlevania: Curse of Darkness, statically recompiled
into 64 bit executables for Windows and Linux. Rendering runs on Direct3D 12 (on Linux, through vkd3d on Vulkan) and
sound on XAudio2 (on Linux, through SDL).

**Thanks to Calm-Consideration41** (PlatinumMimic, who also made the
[Castlevania PS2 4K texture packs](https://buymeacoffee.com/platinummin/castlevania-ps2-4k-texture-packs)) for the
field of view, draw distance and fog settings, the HUD in the screen's corners in widescreen, the PS2's stick
response, the inverted camera and the rumble on both motors, and for 1.8.0's HD movies, material maps and surface
depth, HDR, anti-aliasing, upscaling and frame generation by AMD FSR and NVIDIA DLSS, ambient occlusion, 16:10
widescreen, the settings menu by controller and the launcher's new pages.

This is a personal, non-commercial fan project. It has nothing to do with Konami, Microsoft or any other company: it
is not affiliated with, endorsed or sponsored by them, and it is not an official release. See [Legal](#legal).

> [!IMPORTANT]
> **You need your own copy of the game.** This project does not include, and will not provide, the game: no `.iso`
> of the disc, none of its files and none of its code. The port plays from an `.iso` (or the extracted files) of
> your own USA or European Xbox disc; see [Required Game Version](#required-game-version).

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

- **Linux** - a native Linux build of the game and the launcher, with everything below, as a download of its own
  beside the Windows one. The same Direct3D 12 renderer draws through [vkd3d](https://gitlab.winehq.org/wine/vkd3d)
  on Vulkan, in an SDL window (X11 or Wayland), and the first start builds the game code from your copy as on Windows.
  See [Linux](#linux).
- **Plays from any copy of the USA disc** - an `.iso` (an xiso, or a whole disc dump of either disc layout) or the
  extracted files. Copies whose xbe header was patched for a hard disk loader or another region play too, and the
  launcher tells which of your copies will.
- **The European release too** - with everything below, at 60 FPS, in its five languages (English, French, German,
  Spanish, Italian, set on the launcher's Game page). It has an executable of its own, `CastlevaniaEU.exe`, which
  `Castlevania.exe` starts by itself for a European copy: a player just puts their disc image in `assets`.
- **Hector's shadow** - drawn by the Xbox with 4X multisampling, it is back under his feet.
- **Widescreen menus** - the pause menu, the map, the save screens and the panels behind messages fill a 16:9
  screen, and an area's name card keeps its left end.
- **HD texture packs** - `.dds` packs as well as `.png`; eight Xbox textures the PS2 packs know under other names are
  found (with Panda_Venom and Stan21's pack, 96.8% of what the game draws gets an HD picture); cut-out pictures keep
  clean edges at every distance; a tool makes HD pictures for the textures a pack lacks; and the launcher shows
  whether you have a pack, with a switch for it, which the settings menu has too, while playing.
- **No stutter the first time an effect is drawn** - compiled shaders are kept on disk between sessions.
- **Anisotropic filtering without a pack** - it sharpens the game's own textures too.
- **PS2 button layout** - the PS2's button icons (from a PS2 HD pack) and shoulder buttons, as an option.
- **Lament of Innocence bonus** - the Moai the PS2 game gave for a Lament of Innocence save, as an option.
- **Field of view, draw distance and fog** - a wider view, the world drawn farther and its fog pushed out or gone,
  with the painted skies kept as the game shows them. By Calm-Consideration41.
- **HUD at the screen's corners** - in widescreen the health bar and the rest of the HUD sit in the corners of the
  screen, not of the 4:3 middle. By Calm-Consideration41.
- **PS2 stick response, inverted camera and rumble** - Hector walks any way the stick points, as on the PS2, instead
  of snapping to up, down, left and right; the camera's axes can be inverted apart; and both of the controller's
  motors rumble as the game drives them. By Calm-Consideration41.
- **HD movies** - the story movies played from an HD movie pack in place of the disc's (Calm-Consideration41 upscaled
  every one), fitted to the screen or filling it, switched on and off in the launcher or while playing. See
  [HD Movies](#hd-movies). By Calm-Consideration41.
- **Material maps and surface depth** - with a texture pack that has them (Calm-Consideration41's v4), stone, wood and
  metal take the light by their normal maps and show depth by their height maps (parallax). By Calm-Consideration41.
- **Anti-aliasing and upscaling** - AMD FSR on any graphics card, or NVIDIA DLSS on a GeForce RTX, smooth the 3D's
  edges over the frames, and can draw it below the screen's resolution and rebuild it, for a higher frame rate on a
  laptop or a handheld; the launcher downloads their files from AMD and NVIDIA. On Linux both do too, on Vulkan: AMD
  FSR 3 built into the game (since 1.8.6; before, this port's own TAA, which stays for a card FSR can't run on) and
  NVIDIA DLSS with NVIDIA's own driver (since 1.8.7). By Calm-Consideration41 (on Linux, this fork's).
- **Frame generation** (Windows) - a frame made between each two the game draws, by AMD FSR 3 or NVIDIA DLSS, for twice
  the frame rate on a fast screen. By Calm-Consideration41.
- **Ambient occlusion** - soft shadow where surfaces meet, with anti-aliasing or without. By Calm-Consideration41.
- **HDR** - fire, magic and light brighter than the rest on an HDR display, on Windows and on a Linux desktop in HDR.
  By Calm-Consideration41 (on Linux, this fork's).
- **16:10 widescreen** - for the Steam Deck and 16:10 monitors, beside 16:9. By Calm-Consideration41.
- **The settings menu by controller** - LB and RB turn its pages, B and Start close it. By Calm-Consideration41.
- **Sprint** - a click of L3 (the left stick pressed in) or of Left Shift while Hector walks, and he sprints until the
  stick is let go: 1.25, 1.5 or 2 times his walk, off by default. Only his walk speeds up.
- **Crazy mode and Trevor from the start** - the name codes `@CRAZY` and `@TREVOR` work without beating the game first.
- **Launcher** - in the game's own look, with the title logo from your HD pack. It chooses the graphics card and says
  whether it has Direct3D 12, and has the button layout, the bonus and the pack switch.
- **Launcher in seven languages** - English, Deutsch, Español, Français, Italiano, Português (Brasil) and Русский,
  from a dropdown.
- **Updates from the launcher** - each time it opens, the launcher looks up the newest release on GitHub and, when it
  is not the one in your folder, offers it on the Home page: one click downloads the release, checks it, puts the
  program files in place (the exes, the launcher, the compiler folder, the notes) and restarts the launcher. Your
  saves (`hdd`), `castlevania.toml`, textures, movies and game files are never touched; the files replaced are kept
  in `update\previous` until the next update. The game builds its code again on its first start after an update.
  The check is a switch on the Game page (`update_check`); `update_prereleases = false` in `castlevania.toml` leaves
  releases marked pre-release out. The release a folder holds is `version.txt` beside the launcher, which
  `release.bat` writes into the package; a build folder without one is never offered an update. Windows only for now.
- **Program icon** - the game's crest, made from your copy when you build.
- **Newer recompiler** - Xbox XBE Recomp moved up to its commit `91146ff`, this fork's changes kept as a patch.
- **Requirements written out** - what a PC needs to play, down to which graphics can run it.
- **Automated tests** - a runner that plays scenarios and checks the game state, warps to any room and sweeps every
  room; a bot that replays routes recorded from real play and wins the fights on them; per-room timings; an AI judge
  of screenshots; and a test that installs a release as a player would. See [Testing](#testing).

## Required Game Version

The USA and the European releases are supported, each by an executable of its own, as each config is tied to the
exact code addresses in that release's `default.xbe`. `Castlevania.exe` plays the USA release and starts
`CastlevaniaEU.exe` by itself when the game it finds is the European one (the launcher's **Play** too), so either copy
is just put in `assets`. Many copies have the xbe's certificate patched (its title, allowed media and region, for a
hard disk loader or any region's console); the code is the same, and the game puts those fields back as the release
has them and plays that.

| | USA | Europe |
|---|---|---|
| Game | Castlevania: Curse of Darkness, original Xbox, **USA (NTSC-U)** | the same, **Europe (PAL)**: En, Fr, De, Es, It |
| Title ID | `4B4E002D` (KN-045), version 1, XDK 5849 | `4B4E002D` (KN-045), version 3, XDK 5849 |
| `default.xbe` size | 7,352,320 bytes | 7,385,088 bytes |
| `default.xbe` SHA-1 | `423BC19B7CED263570DCE5E7CD5D81954DFBE13C` | `B1910AAA4897CC9CA9627190289BF18D6A0943BD` |
| `default.xbe` MD5 | `92176FDDF071667F596680DF64DED7FE` | `3AD73BED2302CB60EFEED4E8721959AB` |

The European release plays at 60 FPS (the console it sees is set to 60 Hz) in the language set on the launcher's
Game page: English, French, German, Spanish or Italian, at first the launcher's own language when it is one of them.
Its text, menus, subtitles and opening movie follow; the voices are the same in both releases. Other releases
(Japan, and the PS2 versions, a different machine) will not work. The launcher's Game files page tells which of your
copies will play, and reads the xbe inside a disc image too.

## Installation

1. Download the latest release from [Releases](https://github.com/jmafort/CastlevaniaCOD-PC/releases) and extract it
   anywhere.
2. Run `Castlevania.exe`. The `assets` folder opens.
3. Put your game in it: an `.iso` of your own disc (a whole disc dump or an xiso), or the extracted disc
   (`default.xbe` with its `data`, `movie` and `sound` folders). This project does not provide it.
4. Run `Castlevania.exe` again. The first start builds the game code from your copy, which takes a few minutes and
   shows its progress; press **Start** when it is done. After that the game starts straight away.
5. Optional: an HD texture pack, see [HD Textures](#hd-textures), and HD movies, see [HD Movies](#hd-movies). The game
   plays as it is without them.

A release contains no game code. `Castlevania.exe` holds the runtime, and the first start makes `Castlevania.dll`
from your own `default.xbe` with the recompiler, [Clang and LLD](https://llvm.org) in the `compiler` folder. The
Visual C++ runtime comes with it, so a fresh Windows needs nothing else.

On a freshly installed Windows there is nothing to download besides the release and a graphics driver; see
[Requirements](#requirements).

- `CastlevaniaLauncher.exe` has every setting and checks your game files; `Castlevania.exe` skips it.
- The game can run from the extracted files in `assets`, or from an ISO of the disc placed in `assets`.
- Recomp settings can be adjusted in `castlevania.toml` beside the exe. Saves go to `hdd\E\UDATA\4b4e002d`.

### Linux
1. Download the `-linux.tar.xz` of the latest release from
   [Releases](https://github.com/jmafort/CastlevaniaCOD-PC/releases) and extract it anywhere you can write to, your
   home folder for one (`tar xf CastlevaniaCOD-PC-<version>-linux.tar.xz`).
2. Run `./Castlevania` in the `CastlevaniaCOD-PC` folder (or double-click it). The `assets` folder opens.
3. Put your game in it, as on Windows: an `.iso` of your own disc, or the extracted disc.
4. Run `./Castlevania` again. The first start builds the game code into `Castlevania.so` with the recompiler, Clang
   and LLD in the `compiler` folder (LLVM's own Linux build), which takes a few minutes or less and shows its
   progress; press **Start** when it is done.

`./CastlevaniaLauncher` has every setting, in the same pages and seven languages as on Windows. `lib` holds vkd3d,
which draws the game's Direct3D 12 with Vulkan, and FFmpeg, which plays the HD movies; `fonts` the launcher's fonts
(URW's P052 for its Palatino, and Material Icons). Saves go to `hdd/E/UDATA/4b4e002d`, logs to `logs`. Nothing needs
installing besides a Vulkan driver (see [To Play on Linux](#to-play-on-linux)), and no root: an HD movie pack plays
with the FFmpeg in `lib`, and anti-aliasing and upscaling are AMD FSR 3, built into the game, which needs nothing either. HDR needs a
desktop with HDR on, on Wayland: KDE Plasma 6, GNOME 48 or later, or a Steam Deck's game mode (gamescope). Frame
generation is Windows only for now: AMD's and NVIDIA's libraries are Windows ones.

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
| [CastlevaniaCOD Textures v4](https://drive.google.com/file/d/1tCugvgp1PajfEywF7TVNS7811LgusHlR/view?usp=sharing) | PlatinumMimic (Calm-Consideration41) | On Google Drive, a 12 GB zip. Made for this port: with material maps (normal, ORM and height) for the light and surface depth. |

To add one:

1. Download a pack and extract it.
2. Copy its pictures into a `textures` folder beside `Castlevania.exe`, with the pack's own folders
   (`textures\SLUS-21168\replacements\...` is fine). The launcher's **Open textures folder** makes that folder.
   Keep one pack in it at a time.
3. Open `CastlevaniaLauncher.exe`. Its home page says how many pictures it found, and its switch turns the pack on or
   off; so does the settings menu's (F10) **HD texture pack**, while playing.

`.png` and `.dds` pictures are both read. Pictures of a whole texture are used; the pieces of a texture some packs
also have (`-rWxH` in their names) are not, nor pictures of the PS2's own screen buffers. More on how pictures are
matched, and on the textures the PS2 packs lack, under [HD Texture Packs](#hd-texture-packs).

A pack with material maps (Calm-Consideration41's v4 has them, in its `materials` folder) gives its pictures light and
depth: the launcher's Textures page has **Material maps** (on by default) and **Surface depth** (Off, Low, High: how
deep the height maps look, on the settings menu's Graphics page too).

## HD Movies

Optional. Without them the game plays its own movies, as on the Xbox. Calm-Consideration41 upscaled every story movie
of the game, and his pack plays them in their place. It is his work, made from the game's movies: it is not part of
this project, and is linked here with thanks.

| Pack | Made by | Notes |
|---|---|---|
| [CastlevaniaCOD-PC HD movies v1](https://drive.google.com/file/d/1V7aZ4XW-ofoc3hzCt0urbAx1yhB9NZkQ/view?usp=sharing) | Calm-Consideration41 | On Google Drive, a 3.9 GB zip. |

To add it:

1. Download the zip and extract it.
2. Put its `movies` folder beside `Castlevania.exe` (on Linux beside `Castlevania`), so there is a
   `movies\movies.toml` with the movies beside it.
3. That is all: the next movie plays in HD. The launcher's View page (**HD movies**, **Movie size**) and the settings
   menu (F10) turn them off and on and set their size: **Fit** the whole picture as large as the screen shows it,
   **Fill** the whole screen (the picture's edges cut), **Original** the game's 4:3. A change applies at once, a movie
   playing too.

On Windows they are decoded by Windows' own Media Foundation, on the graphics card where it can; the N editions of
Windows lack it until Microsoft's Media Feature Pack is installed. On Linux FFmpeg decodes them, from the release's
`lib`.

## Features

### Launcher
`CastlevaniaLauncher.exe` checks your graphics (whether they have the Direct3D 12 the game draws with, as the game picks
them: the high performance GPU first; `CV_NO_D3D12=1` shows the warning anywhere) and your game files (the USA or
European `default.xbe` by its hash, or a disc image, each copy named by its release), adds a disc image to `assets` for
you (a hard link on the same drive, otherwise a copy), and has every setting: the graphics card (Automatic, the
fastest, or any with Direct3D 12: on a laptop the one in the processor saves battery), display mode, resolution,
v-sync, widescreen, render scale, anisotropic filtering with quick presets, the HD texture pack, FPS counter, pause on
focus loss, the settings menu key, the button layout and the European release's language. Changes are saved as you
make them; **Play** starts the game, with the executable of the release it found. With copies of both releases in
`assets`, a list beside **Play** chooses which one it starts (`release = "CastlevaniaEU"` or `"Castlevania"` in
`castlevania.toml`, which `Castlevania` and `CastlevaniaEU` started by themselves follow too, handing the game to the
other's executable; without it each plays its own release, the USA one by default). Saves and logs are a click away.
`--page controls` (or home, display, graphics, view, textures, game, files) opens it on that page. It looks like the game's menus: navy with fog
and a teal band, page names on orange banners, the chosen option on a green bar, boxes framed in light blue, Palatino
for the game's serif. Its home page has the title logo from your HD texture pack (else the name in gold), the game's
crest (made from your own copy, as the icon below is: on Linux and in a release, which have no icon, the launcher
makes it itself), and under **Play** the pack's row: how many pictures the `textures` folder has, with the switch for them,
or a button that opens the folder when it has none. The files are looked at again whenever the launcher comes back to
the front, so a pack copied in meanwhile shows.

The launcher speaks English (the default), German, Spanish, French, Italian, Brazilian Portuguese and Russian: the
dropdown with the globe at the bottom of its sidebar switches it at once (`launcher_language` in `castlevania.toml`:
`en`, `de`, `es`, `fr`, `it`, `pt-BR` or `ru`; **Reset to defaults** keeps it). The game itself and its in-game
settings menu stay in English. Every phrase is in `src/launcher/words.h`, a row per English phrase with its
translations; a phrase the launcher shows without a row there does not compile, nor does a row that lacks a language.
`--check-fonts` checks that every letter of every phrase, in each language, is in the fonts the launcher writes it
with (a missing one shows as ?) and says which are missing; `install.py` runs it on every release.

### Settings Menu
Press **F10** in game for the settings menu, or click both sticks in (**L3 + R3**) on a controller. The key can be
changed in the launcher (Game tab), for keyboards where F10 needs Fn or is taken by another program. Every change
applies at once and is saved to `castlevania.toml`. Its pages are the launcher's (Display, Graphics, View, Game,
Controls, Tools), each in the launcher's sections. While the menu is open the game gets no input but a controller's
right stick, which turns the camera, to look at what a setting changes; Escape, the menu key or L3 + R3 closes it. A
controller runs it all: the d-pad (or left stick) moves and A chooses, LB and RB turn its pages, B or Start closes it,
and the buttons held as it closes do not reach the game. By Calm-Consideration41.

### Display
- **Any Resolution** - Renders at your display's resolution instead of 640x480.
- **Display Modes** - Fullscreen, borderless or windowed, with vsync.
- **Widescreen** - On a 16:9 or 16:10 display (16:10 for the Steam Deck's 1280x800 and 16:10 monitors: **Widescreen**
  16:9 or 16:10) the game is shown in that shape with more of the world at the sides (Hor+), without stretching. The HUD, text and movies stay 4:3 in the middle; the backdrops of the menus, the map and the
  save screens reach the sides, as does the dimmed panel behind a message. Turn it off in the menu (Graphics) for
  the original 4:3 picture.
- **HUD at the Screen Edges** - In widescreen the health bar, its numbers, the icon at the bottom left and the
  message box at the bottom right sit in the screen's corners, as they do on a 4:3 screen, instead of at the corners
  of the 4:3 middle (on by default; `hud_edges = false` keeps them in the middle). Menus, the map and story scenes
  stay as they are.
- **No Overscan Streaks** - The rows a TV's overscan used to hide at the top and bottom of the picture no longer
  smear; the frame fills the screen.
- **HDR** - On a display with HDR on in Windows, or in a Linux desktop (Wayland: KDE Plasma 6, GNOME 48 or later, a
  Steam Deck's gamescope), fire, magic, light and the other brightest parts of the picture shine brighter than the
  rest, up to the peak brightness set; the scene brightness sets the rest. On Linux the game sends HDR10 and tells the
  desktop its peak and scene brightness; the desktop does not tell the display's own peak, so 1000 nits stands for it
  until one is set. By Calm-Consideration41 (on Linux, this fork's).

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
textures, which put in `textures` cover the rest of that sweep. The switch applies at once, the launcher's before
**Play** or the settings menu's while playing (a game started without the pack reads its pictures' names in the
background first); the size pictures load at applies from the next start.

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
  newer, Intel Arc), rays through the room itself (`ray_tracing`: 0 off, 1 ambient occlusion, 5 ray tracing, the
  path traced lighting below; bits 1 and 4; launcher Graphics page or the in-game menu; anti-aliasing on).
  Reflections (bit 2) are no longer offered: an
  older setting of 7, lighting and reflections, is read as 5. Each frame, the vertices the game's own draws put on
  the screen are gathered into an acceleration structure (the world as drawn, characters posed as they are), with
  each draw's picture and texture pack material. Cut-outs (railings, leaves, grates) are seen through where their
  pictures are.
  - *Ambient occlusion* is traced instead of guessed from the screen: each pixel sends rays over its surface, so what
    is off screen or behind something still shades, and an edge's shadow does not end where the screen's view of it
    does.
  - *Lighting*: the light the room's surfaces throw onto each other. Each pixel, at half the 3D's size, sends two
    rays over its surface, takes on the colour of what they meet as the game drew and lit it, and throws it back in
    its own colour: the courtyard's water lights the carved wall and gargoyle above it in blue, a torchlit wall warms
    the floor before it, Hector picks up the light around him. A filter across its neighbours on the same surface
    smooths it, and the anti-aliasing gathers it over the frames. It comes with the ray traced ambient occlusion.
    It takes the place of the flat fill the game's artists lit each room with rather than adding to it: a surface
    gets the light it gets above the room's average (the room's over the frames since it came in, so a turn of the
    camera does not pump it), raised in the light's own hue against the room's (a surface that gets more of the room's own
    light is raised in white: the room's colour is in its fill already), and gives up a share of the game's own as it
    gets less (by the square of how far short it falls, so what the rays reach little, a corner or a cliff, keeps its
    picture's detail rather than going to black), so a room keeps the brightness the game gave it while its light
    comes from where the rays say: the glow by the water, the shade in a corner. **Lighting
    strength** (`ray_tracing_lighting`, 0% to 200%, 100% by default) sets how strongly it shows. Reflections take the
    place of the share of a surface's own colour they stand for (its Fresnel share: little head on, most at a glancing
    angle) rather than adding to it, too.
    - *Lights*: what the game adds onto the frame as light, a torch's flame, a glow, a spell in Hector's hand, lights
      the room too. Each pixel samples one of them a frame (picked among a few by how strongly each lights it) with a
      ray toward it for its shadow, so a character between a torch and the wall throws one. Each light is known from
      frame to frame by where it is (up to 128 of them), whatever picture it is drawn with and in whatever order: its
      light grows in over about half a second once it has been there a third of one, so a sword's swing, a hit's
      sparks and a spell's burst throw none while a torch, a lamp and a lava golem's glow throw all of theirs; the
      point a pixel settled on follows its light as others come and go; and a light that leaves the screen goes on
      lighting what is in view for twenty seconds, so a torch just out of view still lights the wall beside it (the
      twelve brightest of those remembered: every pixel weighs each of them, and a save room's hundreds of sparkles
      drifting out of view had filled all 128 and made the lights most of the lighting's cost there).
      Flames and glows the game draws as flat sprites are put back into the room at their own depth (the game depth
      tests them against the room, so that is where they are: a flame at its bracket, a brazier's fire on its stand),
      four fifths as bright as the room's own (an effect more than a lamp: a cloud of motes tints the wall behind it);
      a haze over the whole view is no light, and the glow a character is drawn with (the embroidery on Hector's coat)
      is its look, not a lamp. Their light is light above the room's own, so the room's average leaves it out: a torch
      brightens its wall without the rest of the room dimming to make up for it. Which light each of a pixel's
      candidates is on is picked for a tile of 8 x 8 pixels together, the point on it for the pixel (each pixel keeps
      its own choice from frame to frame): a wave of pixels then reads one light's vertices and picture rather than
      one each, which in the save room, 94 lights listed, was half of the lighting's cost. The bounce rays gather
      the light of the glows they pass through on their way (the save room's water glowing on the stone around its
      pools); of a picture drawn as hundreds of sprites the scene holds 48, the listed and then every so many-th of
      the rest, each standing for those between (the 800 sprites overlapped over the pools, and a ray through them
      visited most of their acceleration structure).
    - *Sky*: outdoors, a ray that leaves the room upward brings the sky's colour as the frame shows it (the more the
      higher it heads, as much as the frame shows sky: none indoors), so the open air lights what faces it and an
      arch or a ledge shades what lies under it. The sky as the frame last showed it is held through a room's
      change and fades out over a minute while none is in view (the bridge before the courtyard, entered from the
      courtyard, has its sky from the first frame: measured afresh in each room, its planks were dark looking along
      the bridge and bright once the camera had tilted up past the sky cards to the open sky). It is the open air's
      light over the whole room, so it counts in the room's average (left out, every surface open to the sky lay
      above the average and a drawbridge or a canyon was raised to twice its brightness).
    - *Light shafts*: the air catches the lights' light on its way to the eye: a torch's glow in the air, cut by the
      bars before it. Only lights drawn in 3D light the air (a fountain's spray, motes and a glow's sprite drawn flat
      filled whole rooms with a grainy, sparkling haze). Marched at a quarter of the frame's size, smoothed, gathered
      over the frames and laid over the frame as a glow, never a fog: a little light at most, and less where the frame
      is already bright (a fountain's falling water and a ghost's body are large lights, and their air washed the
      room out). The game's own fog stays as it was (the sky's light in the air is
      left out: outdoors it laid a haze over every open place, which washed the courtyards out and turned the door's
      blur white).
  - *Reflections* (in the renderer, no longer offered in the menus): a surface with a texture pack's material maps reflects what its mirror direction meets, that
    draw's picture lit as the game lit it, blurred as its roughness spreads the rays. The HD pack's maps are rough all
    over (about 0.8, no metal), too rough to reflect anything, so the port gives the pack's iron (railings, gates)
    a metal's gloss and its dressed stone a worn polish that shows at a low angle; a manifest's `reflect_roughness`
    and `reflect_metalness` set a material's own. Water is a smooth mirror with water's Fresnel, rippled by a few
    slow waves laid in the world (they stay put as the camera moves): the courtyard's pool reflects its gargoyles and
    the balustrade above it. Characters' metal shines too: Hector's armour, gauntlets and sword, an enemy's blade and
    gold trims, told by their pictures' colour (grey steel, gold), their cloth, leather and skin left matte.
  - *Character shadows*: Hector, the enemies and the Innocent Devils cast soft shadows on the floor and the walls
    around them (the game drew none), from a light above and a little to one side, the same way in every room, which
    only the characters block, so the ceiling does not shade the room. They come with the ray traced ambient
    occlusion. A character to the rays is a draw by a skinned program, and Hector's sash and the tails of his coat
    besides, cloth the game moves itself and draws as plain meshes: as meshes of the room's they threw red light on
    the floor under him and trailed it as he walked.
  - *Denoising*: with anti-aliasing on NVIDIA DLSS (any GeForce RTX), DLSS Ray Reconstruction takes over from it and
    cleans the rays' grain as it anti-aliases. Each frame it is given what every pixel's surface is, traced through
    the room at the 3D's full size: its picture's colour, its normal (bent by a texture pack's normal map), roughness
    and specular colour, and how far its reflection's ray went. It takes the light as the frame's colour over the
    surface's own, so the bounce lighting's haze is smoothed while the stone's carving stays as sharp as its picture.
    It needs Streamline's DLSS-RR files (`sl.dlss_d.dll`, `nvngx_dlssd.dll`), which the launcher's DLSS download now
    brings with the rest (a download from before asks to be made again).
    With AMD FSR (any card), or DLSS without those files, the port's own denoiser takes all three before the
    anti-aliasing. The bounce lighting goes through the port's own with Ray Reconstruction too: the lights' samples
    left to it alone flickered on the walls they lit. Each surface's light is gathered over the frames, followed to where the frame's motion says it was
    a frame before (taken only from the same surface: what a character or the camera uncovers starts afresh, so
    nothing trails behind them):
    - the bounce lighting over up to 24 frames, then smoothed across its neighbours on the same surface by how much
      it still varies, a little where the frames have settled it, more where they have not;
    - the ambient occlusion over up to 16, held within what the pixel's neighbours show this frame, so a character's
      contact shadow moves with them rather than trailing behind;
    - the reflections over a few frames on a mirror-like surface and up to 12 on the roughest that still reflects
      (a reflection slides over its surface as the eye moves, so a sharp one keeps little history and needs little),
      held within this frame's neighbours the same way, then smoothed as widely as the surface's roughness blurs it.

    Together they cost about 0.55 ms at 1080p on a GeForce RTX 5080.
  - *Sampling*: the rays' directions, the lights' candidates and the shadows' points are spread rather than drawn at
    random (a low-discrepancy sequence from a blue-noise start of each pixel's own, each frame's points after the
    frame before's), so a texel's few rays cover its hemisphere and its neighbours' errors cancel, which the
    denoisers settle faster. The eye's ray through each pixel is traced once, by the guides, and the ambient
    occlusion and bounce lighting take their surface from it. Above 1080p's pixels the ray traced ambient occlusion
    is worked out at half the 3D's size, with 8 rays a pixel (`XBE_RT_AO_SCALE` sets it): at 4K its rays from every
    pixel took most of the ray tracing's time, and the bounce lighting's rays were halved to make room. A
    character's shadow by a lamp falls from the lamp the bounce lighting has settled on for that surface, so it
    holds still rather than flickering between two lamps.

  All of it costs about 3 ms a frame at 1080p on a GeForce RTX 5080 (the screen space ambient occlusion 0.4 ms), and
  about 3.7 ms at 4K with DLSS Balanced.
  The settings' **Ray tracing** choice is the path traced lighting (`ray_tracing_quality` 3, the default). The key
  in `castlevania.toml` alone, not a control, also takes 0 low, 1 medium and 2 high, for slower graphics cards
  (AMD's, Intel's, a handheld's): low lights the room with one ray a pixel at a third of the frame's size, reaches
  less far, sends fewer ambient occlusion rays at half the frame's size, reflects only the smoothest surfaces and
  leaves the light shafts out (about 1.1 ms on the RTX 5080 at a handheld's 1920x1200); medium is between; high is
  the bounce lighting above, one bounce. **Path traced** does all High does and lights the room as a path tracer
  would: one path a pixel at 0.71 of the frame's size, each followed two bounces through the room. At each place a
  path lands it takes the light of one of the room's lights (a torch, a brazier, a glow, picked by how strongly it
  lights that place, with a ray toward it for its shadow) and of the sky where a ray leaves the room, and where it
  ends it takes the light the game's artists painted there, standing for every bounce after (at the second surface
  it meets, or the first where the second ray leaves the room). The game's own flat light is then kept only an
  artists' share (0.45 at Lighting strength 100%, less as the strength rises), and the rest of each surface's
  brightness is what the paths brought set against the room's average, in the light's own hue: a brazier's purple
  spreads over the floor around it and fades with the distance, a corner that nothing reaches goes dark, and the
  light falls off from the lamps as light does. About 4 ms a frame at 4K with DLSS Balanced on the RTX 5080 in the
  castle's rooms (High's 3.5), 6.5 in the Abandoned Castle's save room, the costliest room (a 4K frame there 11 ms;
  it was 19.5 before the lights' sample picked per tile and the glows' sprites were thinned, above). A room the
  rays find no light in at all (one whose walls are skinned meshes, which the bounce rays pass) is left as the game
  drew it, at every quality.
  The frame's scene goes into the acceleration structure as a few merged geometries (the world's solid draws, its
  cut-outs, its glows, the characters', the water), built back to back: a thousand geometries a frame had cost the
  build more than its triangles. `perf-run.bat` beside the game starts it with the SDK's timings on and leaves
  `logs\perf-report.log` when it is quit: how long each part of a frame takes on that machine, the frames the
  ray tracing's scene was built for, and the room's average the bounce lighting holds. That average is eased from
  frame to frame, so one pixel of one frame that came out not a number would have held it there, and the lighting
  off, until the ray tracing was turned off and on: the passes keep such pixels out, and the average is read back
  each frame and the lighting started over should it ever go bad (said in the log).
  The shaders are compiled to DXIL by the Windows SDK's `dxc` when the port is built; a build without it, or a card
  without DXR 1.1, keeps the screen space ambient occlusion.
- **Field of View** - 100% (the game's, which each room's camera sets) to 200%, a wider view as the number goes up;
  the in-game menu shows the view's angle as you move it. It holds through a room change's fade-in; story scenes
  keep the game's framing (`fov = 130`).
- **Draw Distance and Fog** - How far the world is drawn (the game's, 1.5x, 2x or 4x) and how far out its fog and
  darkness start (the game's, 1.5x, 2x, 4x, or none). **Far View** sets both to 4x (`draw_distance = 400`,
  `fog_distance = 400`). Rooms the game fogs heavily open up most; a room's far scenery can end where its fog used to
  hide it. The skies are painted strips, clouds and moons the game placed far out (the Abandoned Castle's rampart
  has a moon 45,000 units out, its fog ending at 39,000); they look as the game shows them with the fog pushed out
  too, their pictures repeating across them as on the PS2 (before 1.8.0 they were read stretched, which showed as
  smears and the strips' edges). Some skies also have glow cards that the game's own view never shows, which pushed
  out showed as faint squares over the moon and the clouds; with the view pushed out they are not drawn (1.8.4).
- **Anti-Aliasing** - Smooth, steady edges on the 3D, gathered over the frames: AMD FSR on any graphics card, NVIDIA
  DLSS on a GeForce RTX (from the game's next start once chosen), their files downloaded by the launcher's Graphics
  page from AMD and NVIDIA (the game loads them only signed by AMD or NVIDIA, the launcher checks their SHA-256). On
  Linux AMD FSR 3 on Vulkan, built into the game from AMD's FidelityFX SDK (an unofficial Linux build of its 1.1.4,
  patched by this port), on any graphics card; where it can't run, this port's own TAA in its place
  (`XBE_TAA=1` chooses that). And NVIDIA DLSS on Vulkan through NGX, which NVIDIA's own Linux driver brings, on a
  GeForce RTX: the launcher's Graphics page downloads NVIDIA's DLSS SDK files from NVIDIA (about 60 MB, each checked
  against its SHA-256), and the game links itself to them on your PC with its compiler folder's clang, as it builds its
  code, so this port's download carries nothing of NVIDIA's. On another driver (Mesa's NVK, or an AMD or Intel card)
  AMD FSR takes DLSS's place. The HUD and menus stay sharp. By Calm-Consideration41 (on Linux, this fork's).
- **Upscaling** - The 3D drawn below the screen's resolution and rebuilt to it by the anti-aliasing: Quality (two
  thirds across), Balanced or Performance (half), for a higher frame rate on a slower graphics card or a handheld.
- **Sharpening** - FSR's and the TAA's, 0 to 100% (30% by default).
- **Ambient Occlusion** - Soft shadow where surfaces meet, Low or High, from the 3D's depth: with anti-aliasing it is
  smoothed over the frames too. By Calm-Consideration41.
- **Frame Generation** (Windows) - A frame made between each two the game draws, AMD FSR 3 on any graphics card or
  NVIDIA DLSS on a GeForce RTX 40 or 50, from the next start: about twice the frames reach the screen while the game
  keeps its 60. On a screen whose refresh is not a multiple of 120 Hz (60, 144, 165) its frames go without v-sync,
  as with it AMD's would hold the game under 60 and slow its play; a G-SYNC or FreeSync screen shows them without
  tearing. By Calm-Consideration41.
- **Material Maps and Surface Depth** - See [HD Textures](#hd-textures). By Calm-Consideration41.
- **Shader Cache** - Shaders compiled while playing are kept in `cache\shaders` beside the game, so later sessions
  do not stutter the first time an effect is drawn.

### General
- **FPS Counter** - Optional, in the corner.
- **Pause on Focus Loss** - The game stops while another window is in front (on by default), and so does its sound,
  the looped ones too (the first courtyard's fountain).
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
  `button_layout = 1`; it changes at once, in game too). The PS2 icons come from an HD texture pack made from the PS2
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
- **Sprint** - Click the left stick in (L3), or press Left Shift, while Hector walks: he sprints until you let go of
  the stick, at 1.25, 1.5 or 2 times his walk (off by default; the launcher's or the in-game menu's Controls page;
  `sprint = 150`). Only his walk speeds up, its animation with it, so his steps keep up with the ground: his guarded
  walk, the dodge, jumps and attacks, every enemy and the game's clock keep their pace. The game has no use of its own
  for the stick's click, in either button layout; L3 with R3 is still the settings menu.

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
| Left Shift | sprint (L3 on a controller) |
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
  image, 2.9 GB extracted (a whole dump of the European disc is 7.8 GB).
- **Your copy of the game**, the USA or the European release (see [Required Game Version](#required-game-version)).
- Optional: a controller (Xbox, PlayStation or Switch; the keyboard plays too), an HD texture pack and HD movies (on
  the N editions of Windows with Microsoft's Media Feature Pack).

Everything else comes in the zip or with Windows. The zip has the Visual C++ runtime (`msvcp140.dll`,
`vcruntime140.dll`, `vcruntime140_1.dll`) and the recompiler, Clang and LLD the first start builds the game code
with; Direct3D 12, the shader compiler (`d3dcompiler_47.dll`), XAudio2 and the C runtime (UCRT) are part of
Windows 10 and 11. Playing needs no internet connection, the first start neither.

### To Play on Linux
- **Linux**, 64-bit, with glibc 2.35 or later (Ubuntu 22.04 or later, Debian 12, Fedora, Arch, SteamOS and the like),
  on X11 or Wayland.
- **GPU** with a Vulkan driver: your distribution's Mesa drivers for AMD and Intel, or NVIDIA's own. The same graphics
  as on Windows run it (NVIDIA GeForce 600 series, AMD Radeon HD 7700, Intel HD 4200 and newer).
- **CPU** with SSE4.2 and POPCNT (x86-64-v2), as on Windows.
- **Disk**: about 0.4 GB for the release (its compiler folder is most of it), and 0.2 GB more once the first start
  has built the game code (it wants 1 GB free to start; its ready page can delete the compiler folder afterwards),
  besides your game.
- The C++ runtime (`libstdc++.so.6` of GCC 12 or later) and `libz.so.1`, which every desktop Linux has.

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

- [Python](https://www.python.org/downloads/) 3.11 or newer (tested with 3.14), with **Add python.exe to PATH**
  ticked in its installer. The tests use its standard library alone. On Linux also Xvfb, xdotool, ImageMagick and
  Weston (the regression suite's Wayland test).
- For `--judge` and a step's `"ask"`: the [Codex CLI](https://github.com/openai/codex), signed in. For
  `--judge=jev-latest`: a TypeSafe API key.

## Building

On Linux, see [Building on Linux](#building-on-linux).

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
(`tools\extract_iso.ps1`, a minute or less), as the build reads `default.xbe`. That is the USA release's.

The European release's executable is built too when its `default.xbe` is in `assets\eu` (out of its disc image, for
its SHA-1 and its code; `config\game_eu.toml` is its config). To play it from the build, put its disc image in
`assets` beside the USA files: `CastlevaniaEU.exe` finds it there. `release.bat` needs both xbes.

### 4. Build the Project
Run `build.bat`. The first build recompiles the game and takes a while. `Castlevania.exe` and
`CastlevaniaLauncher.exe` are written to `out\build\release`, with the game code built in. Their icon, made by
`tools\xbe_icon.cpp`, is the game's crest in white on a black disc. It comes from your HD texture pack's 512x512
picture of it (the PS2 menu's, with its ring of runes at the larger sizes), or else from the crest on the Xbox
dashboard, 128x128 in your `default.xbe`. A release has no icon, as it packs nothing of the game; its launcher makes
the same crest from your files when it starts (`src/common/crest.h`, which the icon is made with too), from the pack's
picture or your copy's `default.xbe` (in `assets`, or in a disc image there), as the Linux launcher does.

### 5. Make a Release
`release.bat 1.0.0` writes `out\release\CastlevaniaCOD-PC-1.0.0.zip`: `Castlevania.exe`, `CastlevaniaEU.exe` and the
launcher, built in `out\build\package` (the `package` preset: no icon, and no DLL, the game code), the `compiler`
folder the exe's first start builds the game code with on the player's PC (without its `work` folder; both releases'
configs and import libraries), and the Visual C++ runtime. It checks that nothing of the game went into the package. `tools\cvtest\install.py` then tests the zip as
a player would install it (see Testing).

Before every release the whole regression suite passes on both platforms, on the commit released and with its
packages: `python -I tools\cvtest\suite.py --linux` runs it on Windows and then on Linux in WSL, and
`python -I tools\cvtest\suite.py --check` says whether both passed on what is checked out now (see Testing).

### Building on Linux
With Clang, LLD, CMake 3.25 or later, Ninja and the headers SDL needs for X11, Wayland and sound (on Ubuntu:
`clang lld cmake ninja-build pkg-config libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev
libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev libegl-dev libgl-dev libpipewire-0.3-dev
libpulse-dev libasound2-dev libudev-dev libdbus-1-dev libvulkan-dev spirv-headers libjson-perl bison flex`), and
FFmpeg for the HD movies: your distribution's (`libavformat-dev libavcodec-dev libswscale-dev libavutil-dev`), or a
build of its own given to CMake with `-DCV_FFMPEG_DIR=<its prefix>` (the release's: FFmpeg 7.1.2 under the LGPL alone,
with only its H.264 and HEVC decoders, MP4 demuxer and swscale; `tools/linux/release.sh` has its configure line):

1. Build [vkd3d](https://dl.winehq.org/vkd3d/source/) 2.1 into a folder of its own:
   `./configure --prefix=$HOME/vkd3d --disable-tests --disable-demos && make && make install`.
2. Clone this repository and the SDK beside it and apply the SDK patch, as above.
3. Put the game in `assets` (extracted; `default.xbe` must be there), as above.
4. `cmake -S CastlevaniaCOD-PC -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang
   -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2
   -DXBE_VKD3D_DIR=$HOME/vkd3d` (and for AMD FSR, `-DXBE_FSR_VK_DIR=<cache>/fidelityfx` once
   `tools/linux/fidelityfx.sh <cache>` has built it; without it the own TAA takes its place), then
   `ninja -C build Castlevania Castlevania_code CastlevaniaLauncher`. The generated
   code goes to `build/generated`, so a Windows build of the same checkout keeps its own. `Castlevania.so` is linked
   beside `Castlevania` with the game code; the game finds `assets` in a folder above it, or link one into `build`.

`tools/linux/release.sh 1.0.0` makes `out/release/CastlevaniaCOD-PC-1.0.0-linux.tar.xz` in a
[Podman](https://podman.io) container of Ubuntu 22.04 (`tools/linux/Containerfile`), so the release runs on glibc 2.35
and later: both executables and the launcher without the game code, vkd3d's and FFmpeg's libraries in `lib`, the
`compiler` folder
with LLVM's own Linux Clang and LLD, and the fonts. It checks that nothing of the game went into the package, as
`release.bat` does. `tools/cvtest/install.py` tests the archive as a player would install it, from Linux or WSL.
WSL's own Mesa has Vulkan on the processor only (llvmpipe, a few frames per second); Mesa built with
`-Dvulkan-drivers=microsoft-experimental` (dozen) draws on the PC's graphics card, picked with `VK_DRIVER_FILES`.

The Linux build is the Windows one's code: `runtime/linux` in the SDK has the Win32, D3D12 and DXGI names the shared
sources use (over vkd3d's headers), a swap chain of Vulkan images the renderer draws into and a blit presents, the
window and main loop on SDL, SDL's sound, PNG and BC pictures (stb), and the first start's build. The game code is a
library linked without a C runtime, as on Windows: the recompiler writes `x86_host_stubs.cpp`, through which the
executable reaches the library's bodies, and the link's version script keeps global only the names the executable
overrides (`cmake/xbe_hooks.cmake`, the list in `compiler/<exe>.hooks`).

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
  grid, which puts Hector's 4X stencil shadow volume back in place (found by Neguchads); another region's release in
  an executable of its own, which the others start for its game files (`GameInfo::releases`, `xbe_releases()`); the
  console language a title reads (`GameInfo::language`); the settings file written over, its other keys kept; a
  target telling whether its picture is the CPU's (`Target::cpu`). The SDK's mixer plays every channel of a voice by
  itself now (cutscene audio is a 5.1 stream with the voices in the centre channel). `kernel::g_file_opened`, a
  game's hook on each file the title opens; a replacement uploaded into the texture of the one before it when it has
  its size. Ray tracing (`runtime\gpu\raytrace.h`, `rt_*.hlsl`): the world's draws captured by their vertex shaders
  into an acceleration structure each frame, ambient occlusion, bounce lighting and reflections traced through it,
  and a material's `gpu::Material::gloss` for reflections. `XBE_MUTE` keeps a test's game silent;
  `XBE_D3D12_DRED` logs where the GPU stopped when the device is lost, `XBE_D3D12_DEBUG` the debug layer's errors.
- Releases are the SDK's own (`release.bat`): the game code is `Castlevania.dll` (`CastlevaniaEU.dll` for the
  European release), which the exe builds on the player's PC on its first start (the SDK's
  `runtime\app\game_code.cpp`, behind a progress window) from their own `default.xbe`, with the config whose `library`
  is that DLL. A DLL whose stamp does not match the exe and xbe is built again.
- Two releases: the European `default.xbe` is the same game built later for five languages, its code and data a
  little shifted (9 KB more code; data 0x2C40 to 0x7980 bytes further on). It is a second image of its own
  (`add_xbe_image(CastlevaniaEU eu/default.xbe game_eu.toml ...)`, its config written by `xbe_symbols`): the same
  sources compiled with `CV_EU`, every address they use taken from `src\common\release.h`, `src\game_eu` the hooks
  (each includes its `src\game` file). The European addresses came from matching the two xbes
  (`tools\release_map.py`): functions by their bytes with addresses and call offsets masked (15,454 of 20,373
  matched), data by what the instructions of matched functions name, and the few reached only through pointers
  (Hector's attributes and gold, the camera's view) checked in play and against the USA release's memory at the same
  moment. The European game resets the device for its present interval (`D3DDevice_Reset`, which the USA release never
  calls and the 5849 signatures do not find), named in its config by hand. The SDK's `GameInfo::releases`
  (`xbe_releases()` in CMake) lists each release's xbe SHA-1, certificate and executable: an executable whose game
  files hold another release's xbe starts that release's executable with its arguments, waits for it and ends with
  its exit code (its log carries on, `--handed-over`). `GameInfo::language` is the console language the kernel gives
  the game: the European release's `game_language`, English for the USA one, whose disc has only placeholders for the
  other languages' files. The SDK's settings file is written over rather than replaced, so a key the game does not
  know (the launcher's own) is kept.
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
- `src\common\widescreen.cpp`, a renderer extension:
  - Widescreen. The game projects with a PS2 style view screen matrix (`c68`, or `c85` with the world matrix) and
    maps PS2 screen space to console pixels with `c94`/`c95`. Each draw into the back buffer is squeezed to 3/4 of its
    width around the centre: for draws that use the camera that is a wider field of view, for 2D draws a 4:3 shape.
    Draws over the whole width stay whole when they are untextured (fades), take a few texels across it (a message's
    dimmed panel) or read a render target the GPU drew (the frame pass); one whose picture the CPU wrote is a picture
    (the European release's intro decodes every other frame of its movie into a target of the game's, which once
    kept it whole every other frame). Menus, the map and the save screens draw their backdrop
    in pieces; the pieces at the frame's edges have those edges pulled out to the screen's (a vertex on the edge is
    not squeezed), unless the 3D world (1000 vertices of it) or a 4:3 picture over the whole frame came first: the
    title's logo is in pieces at the edges, over its castle. `CV_WIDE_TRACE=<n>` logs every draw into the back buffer
    of one frame in n, where it lands and what was made of it. Only draws into the device's back buffer are squeezed:
    the game composes an area's name card in another 640x480 target and copies it out to a texture, and squeezed
    there the card lost its left end.
  - Overscan. The last pass of each frame draws a copy of the frame back with its rows -16 to 496 over rows 0 to 480
    (PS2 448 lines plus border shrunk into the NTSC frame); the European release's rows -16 to 490 and columns -20 to
    660 over the 640, its picture narrowed by 640/680. What reads past the copy clamps into streaks at the edges, and
    the copy's first row and column are left out by the game's 2D draws, a pixel right and down on the PS2's grid
    (a fade misses them: a light line at the top). The pass is drawn where its texels are, the copy's pixel a pixel up
    and left on the frame, so the copy fills the frame pixel for pixel. The European intro's movie is drawn the same
    way; squeezed to 4:3, the columns it reads past its texture are left out (`ShaderHooks::Output`).

### Testing
- `python -I tools\cvtest\suite.py` is the regression suite: every feature of the port by its tests, listed in
  [tools/cvtest/SUITE.md](tools/cvtest/SUITE.md) with what each checks and what only a person can check, run one
  after another on this platform: the scenarios below on the dev build, then the release packages in `out\release`
  as a player installs them (`install.py` with each disc, `launcher_test.py`, `discs.py`). Scenarios that come after
  the same new game play in sessions, one game each (`cvtest.py --session`): the new game once, then each scenario
  from the courtyard where it starts Hector (warped back, healed, menus closed, the settings the one before changed
  put back), each still a test with its own report; `--alone` plays every scenario in a game of its own. `--linux`
  (on Windows) then runs it on Linux in WSL; `--quick` runs a shorter list, `--only` and `--from` some of it. A
  test that needs what the PC lacks (a disc, the HD pack, a package) is skipped, and a run with one skipped is not a
  pass. This PC's discs and WSL setup are in `out\test\suite.json` (its format at the top of `suite.py`). A full run
  that passed on a commit without changes records it (`out\test\suite-<platform>.json`, with the packages'
  SHA-256), which `--check` reads: a release needs both platforms' for the commit released. A feature added gets a
  test there.
- `python -I tools\cvtest\cvtest.py tools\cvtest\scenarios\newgame.json` plays a scenario in a test folder of its own
  (`out\test\<scenario>-<time>`, its own settings and saves) through `CV_PAD_LIVE`, checks the game state after
  every step (`src\common\telemetry.cpp`: map and room, Hector's position, HP, the camera, menus), the log and
  the screen, and writes `report.md` with a screenshot per step. Scenario format: the top of `cvtest.py`.
  `bot.json` has the test bot (`src\common\bot.cpp`) walk Hector to points and fight the knights of room 5.12.
  `--release eu` plays it on the European release (`CastlevaniaEU.exe`, its disc image in `assets`): every scenario
  here passes on both, and `--set game_language=5` (3 German, 4 French, 6 Italian) plays it in another language.
- `python -I tools\cvtest\cvtest.py --record <route> [--save <name>]` starts the game for play (the play folder's
  settings, keyboard and controllers) and records it; once the game is closed the recording becomes
  `tools\cvtest\routes\<route>.json`, a leg per room with the points walked, the jumps and B presses (doors,
  save points), the menus and message boxes as they were answered, and the fights that were won. A scenario step
  `{"route": "<route>"}` plays it back with the bot: `route.json`, and `first-save.json` (a new game to the first
  save room, a save there, the pause menu and the map). `keep-save.json` keeps that save, and `load-save.json`
  continues from it: one release's save loads in the other (both releases' saves are in `hdd\E\UDATA\4b4e002d`, the
  same title), which they do both ways.
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
  the next run with that name (a test starts cold otherwise). `saveroom.json` stands ten seconds in the Abandoned
  Castle's save room, the costliest room the ray tracing has met (its hundreds of glow sprites), and `saveflick.json`
  shoots it six times two frames apart for its flicker; `flesh.json` takes the Garibaldi Temple's flesh room and
  two beside it, which the bounce rays find no light in; `f10room.json` and `qualityswap.json` open the settings
  menu while playing (the latter changes the ray tracing quality in it) and go on to the next rooms.
- A step's `"ask"` puts a question about its screenshot to GPT-6 Luna, with named answers an `expect` can check.
  `buttons.json` asks which controller's icons show at the courtyard door and on the controller settings page;
  `--set button_layout=1` runs it with the PS2 layout (`--set` changes any setting for a run).
- `CV_TEXPACK_MISSES=1` logs each texture the HD pack has no picture for, once, as a pack would name it; set to a
  folder instead, it also saves each there as a PNG under that name, with the PS2's alpha as PCSX2 dumps it, so an
  upscaled copy in `textures` replaces it. `CV_TEXPACK_CENSUS=1` logs every texture the game uploads, once, with
  its format, size and what the pack made of it (a picture, none, or not looked for: the pack has P8 pictures only).
  `CV_TEXTURES=<folder>;<folder>` reads pictures from those folders instead of `textures`, a picture in an earlier
  one in place of one with the same name in a later one: pictures tried out beside a pack without changing it.
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
  have. `CV_UPSCALE_CPU=1` keeps the model off the GPU, as while a game has it. `upscale.py calibrate` measures
  models against the pack's own pictures of the same textures (`kAliases`' and those): of
  RealESRGAN_x4plus, UltraSharpV2, GTAV textures (RGT, and its dither model) and GameAI 2.0, RealESRGAN_x4plus came
  closest by far (31.2 dB on average, 45 dB on one, the others 26.7 to 28.7), the pack likely made with it, so its
  pictures look like the pack's. They are the game's art, as a pack is: they stay out of this repository. The
  menus' text is pictures too (the words, digits and panels of the pause menu's pages, the title screen's menu, the
  loading and save screens: 71 sheets the pack never covered, seen with `CV_TEXPACK_MISSES` on cvtest's
  `pausemenu`), so the same run makes them sharp, with the pack's own redrawn pieces placed in where it has them;
  `tools\texpack\clean_menu_bars.py` then redraws the pause menu's button bars' ends, painted ragged in the game's
  sheets, as a smooth fade (its docstring says which two sheets draw a bar, and how).
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
- `lockon.json` locks on (Black, the game's Lock-On ON/OFF) to the Executioner of room 5.2 with him off the middle of
  a widescreen: its marker, drawn from the HUD's sheet at him, stays where the game puts it while the HUD goes to the
  screen's corners. The telemetry's `wide` says how far widescreen moved the marker and the HUD as last drawn.
- `python -I tools\cvtest\launcher_test.py <release .zip or .tar.xz, or a build folder> <USA disc> <European disc>`
  installs a release as `install.py` does, with one disc and then both, and drives the launcher by window messages (on
  Linux, on a virtual display) where it says its controls are (`CV_LAUNCHER_STATE`): with one disc no list beside
  **Play**; with both the list, the European release chosen and saved, **Play** starting `CastlevaniaEU`, which
  finds its disc; the home page in Russian; and each executable started by itself following `release`, or playing
  its own release without one. A game started is ended once its log says which disc it plays.
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
  draw list (each draw's state, its textures and shaders), as does the live entry `0:capture` at a step's start.

## Known Issues
- Only the first areas have been tested so far, in both releases.
- Linux has been tested in WSL (Ubuntu 26.04) only, on an NVIDIA GeForce RTX 3060 and an Intel Iris Xe through
  Mesa's dozen (Vulkan on Windows' Direct3D 12), on X11 and Wayland: both releases install, build their game code and
  play the new game, widescreen and room tests at 60 frames per second on the RTX 3060, with the same pictures as on
  Windows. It has not been tried on a Linux PC's own graphics drivers yet.
- Linux has no frame generation yet: the Linux build of AMD's FidelityFX SDK has none, and the port's Linux DLSS is
  its anti-aliasing and upscaling only. Its anti-aliasing and upscaling by AMD FSR 3 are new (1.8.6): tested in WSL on
  an NVIDIA card through Mesa's dozen, not yet on a Linux PC's own graphics drivers. Each frame waits for the GPU once
  more with it than with the TAA.
- NVIDIA DLSS on Linux is new (1.8.7) and not yet tested on NVIDIA's own Linux driver: in WSL, where the card is
  Mesa's dozen, its download, the game's link to it and the switch to AMD FSR were tested, and DLSS itself ran there
  through NVIDIA's NGX, but not in the game. If it does not run, `logs/castlevania.log` says why (and NVIDIA's own
  logs are in `logs/ngx`); `XBE_DLSS_LOG=1` adds NVIDIA's lines to the game's log.
- HDR on Linux is new (1.8.5): its picture was tested in WSL, where the desktop has no HDR, but not yet shown on a
  Linux desktop in HDR.
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
- `config\game.toml` and `config\game_eu.toml` list addresses and names of functions in each release's executable,
  as `src\common\release.h` does for the port's own code. They contain no code from it.
- You need your own copy of the game, the USA or the European Xbox disc, as an `.iso` or its extracted files. This
  project does not provide one, nor tell where to get one.
- The HD texture packs linked under [HD Textures](#hd-textures) are their makers' own work, made for the PS2 game.
  They are not part of this repository or its releases, which neither host nor include any of their pictures. The
  same goes for the HD movie pack linked under [HD Movies](#hd-movies), which is made from the game's movies.
- AMD's FidelityFX and NVIDIA's DLSS and Streamline libraries are not in the Windows release either: the launcher
  downloads them from AMD's and NVIDIA's own releases, under their licences, which it saves beside them. The Linux
  release has AMD's FSR 3 upscaler built into the game from the FidelityFX SDK's source (MIT), its licence in
  `THIRD_PARTY.txt`; NVIDIA's DLSS SDK files are downloaded by the launcher from NVIDIA's own GitHub, under NVIDIA's
  licence, which it saves in `licences`, and the game links its bridge to them on your PC
  (`compiler/ngx/cv_ngx.cpp`, this port's, is the one source file in the release).
- The recompiled code is generated on your own PC from your own copy of the game. `Castlevania.dll`, and an
  executable from `build.bat`, are for your personal use only. Do not distribute them, generated code or game files.
- The cover at the top is fan art made for this project with an AI image model, after a screenshot of the port's
  Abandoned Castle rampart. It is not Konami's artwork.
- Castlevania and Curse of Darkness are trademarks of Konami Digital Entertainment. Xbox is a trademark of
  Microsoft Corporation. All other trademarks belong to their respective owners and are used only to identify the
  game this port works with.

The source in this repository is released under the [GNU General Public License v2.0](LICENSE). The SDK patch in
`sdk-patches` modifies
[Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) and is under its MIT License, Copyright (c) 2026 TGP482.
Each release has the licences of everything else in it in `THIRD_PARTY.txt`, and LLVM's in `compiler`.

## Credits

- [CastlevaniaCOD-PC](https://github.com/Pazzi7054/CastlevaniaCOD-PC) by Pazzi7054, the port this repository is a
  fork of.
- [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) by TGP482, the recompiler and runtime this is built on.
- [Castlevania-COD-Recomp](https://github.com/Neguchads/Castlevania-COD-Recomp) by Neguchads, for its findings in the
  game: Hector's shadow on the 4X multisample grid, and the addresses of the camera, maps, rooms and enemies.
- Calm-Consideration41 (PlatinumMimic) for the field of view, draw distance and fog settings, the HUD at the screen's
  corners, the PS2 stick response, the inverted camera and the rumble; and for the HD movies and their pack, the
  material maps and surface depth and the textures v4, HDR, AMD FSR and NVIDIA DLSS anti-aliasing, upscaling and frame
  generation, ambient occlusion, 16:10 widescreen, the settings menu by controller and the launcher's new pages.
- [AMD FidelityFX SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK) and
  [NVIDIA Streamline](https://github.com/NVIDIA-RTX/Streamline) (their headers, MIT) for loading FSR and DLSS.
- [FFmpeg](https://ffmpeg.org) (LGPL 2.1), which plays the HD movies on Linux.
- The HD texture packs' makers: Panda_Venom and Stan21
  ([HD Remaster](https://gbatemp.net/threads/castlevania-curse-of-darkness-usa-hd-remaster.619688/)), VictorKratos
  ([VK Curse of Darkness](https://drive.usercontent.google.com/download?id=1rEeBsGLc93pQerT_7j-MwzT-otxhGZl-&authuser=0))
  and PlatinumMimic, Calm-Consideration41
  ([4K Texture Packs](https://buymeacoffee.com/platinummin/castlevania-ps2-4k-texture-packs)).
- [LLVM](https://llvm.org) for Clang and LLD, which releases bundle to build the game code on the player's PC.
- [vkd3d](https://gitlab.winehq.org/wine/vkd3d) (LGPL 2.1), which draws the game's Direct3D 12 with Vulkan on Linux;
  [stb](https://github.com/nothings/stb) for PNG files there; [Material Icons](https://github.com/google/material-design-icons)
  and URW's [P052](https://github.com/ArtifexSoftware/urw-base35-fonts) (AGPL 3.0 with a font exception) for the Linux
  launcher.
- [xxHash](https://github.com/Cyan4973/xxHash) (BSD 2-Clause) for hashing textures, and [PCSX2](https://pcsx2.net) for the texture
  pack naming its replacement pictures follow.
- [Real-ESRGAN](https://github.com/xinntao/Real-ESRGAN), the model `tools\texpack\upscale.py` makes HD pictures with
  for the textures the packs lack.
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) for the overridable function layout and instruction hooks.
- [Zydis](https://github.com/zyantific/zydis) for decoding x86 instructions, [toml++](https://github.com/marzer/tomlplusplus) for the configs, [Dear ImGui](https://github.com/ocornut/imgui) for the settings overlay and [SDL](https://github.com/libsdl-org/SDL) for the window, input and controllers.
- [xemu](https://github.com/xemu-project/xemu) and [Cxbx-Reloaded](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded) as references for the Xbox's hardware and API behaviour.

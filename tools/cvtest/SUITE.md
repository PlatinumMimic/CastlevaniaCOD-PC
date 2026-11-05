# The regression suite

Every feature of this port and the tests that cover it. `tools/cvtest/suite.py` runs them all, one after another,
on the platform it runs on; a release is made only from a commit both platforms passed in full
(`suite.py --check`). A feature added gets its test here and in `suite.py` with it.

    python -I tools\cvtest\suite.py --linux       Windows, then Linux in WSL (under an hour each)
    python3 -I tools/cvtest/suite.py              Linux alone
    python -I tools\cvtest\suite.py --quick       the shorter list, during work
    python -I tools\cvtest\suite.py --check       both platforms passed on this commit, with these packages

This PC's discs and WSL setup are in `out/test/suite.json` (see `suite.py`), with the folders some tests need: an HD
movie pack (`movies`), a texture pack with material maps (`materials`, PlatinumMimic's v4) and AMD's and NVIDIA's
libraries as the launcher downloads them (`vendor`); without one its tests are skipped, and said so. Tests marked
*Linux* run only there: on Windows they would take the PC's own screen or keyboard. Those marked *Windows* need AMD's
or NVIDIA's libraries, which are Windows ones.

Scenarios that come after the same new game play in sessions (`SESSIONS` in `suite.py`): one game plays the new
game once and then each of them in turn, each from the courtyard where a new game starts Hector, with the menus
closed, his HP full and the settings the one before changed put back (`cvtest.py --session`); a test's own `--set`
is made live for it. Each is still a test of its own, with its own report. A session's order matters: what needs the
game as a new game leaves it goes first (the first courtyard's texture census, the first save point's boxes), and
`menu`, whose Reset to defaults gives every setting the game's own value, last; `lockon` never shares a game with
`widescreen` (its first-save route kills the Executioner for good, and is stuck in his corridor once he is out of
his wall). After a test fails the next plays a fresh game. `--alone` plays every scenario in a game of its own, the way to check a session's results.

## The game

| Feature | Tests (suite names) | What they check |
|---|---|---|
| Boot, a new game, movies, play | `newgame`, `newgame-eu` | title, name, save, movies skipped, the first courtyard: HP, no menu, 50+ FPS, sound played, the window at the size set, walking, the pause menu |
| Sound | `newgame` (and every scenario after it) | the output played 20 s by the courtyard, 5 s of it with sound (telemetry `audio`) |
| Rooms load and draw | `rooms`, `rooms-eu`, `warp` | ten rooms of most maps warped to: no crash, hang, log error or flat colour; the warp to other rooms and maps |
| Fights, the bot, recorded routes | `bot`, `route`, `first-save` | the knights of 5.12 beaten (EXP, a level), a recording played back to the first save room and saved there |
| Saves, both releases | `save-usa`, `load-save-eu`, `save-eu`, `load-save-usa` | a save made by each release continues in the other |
| Crazy mode and Trevor from the start | `crazy`, `trevor` | the name codes typed on a game never beaten: the mode, the save's name |
| Lament of Innocence bonus | `moai-on`, `moai-off` | the Moai floats at the far end of 5.0 with `lament_save`, not without (asked of the judge) |
| The European release in five languages | `language-eu-en`, `-de`, `-fr`, `-es`, `-it`, `language-usa` | the title menu in the language set (asked of the judge); the USA release English whatever is set |
| Hector's shadow (4X multisampling) | `shadow` | the floor beside his legs at the start of 5.2 about half as bright as farther away |
| Widescreen menus and boxes | `widescreen`, `widescreen-eu` | the map screen and the pause menu drawn to the screen's right edge (a 4:3 picture leaves it black); the boxes of the first-save route kept as pictures |
| HUD at the screen's corners | `qol`, `lockon`, `lockon-eu` | the HUD moved to the edge with `hud_edges`, not without; the lock-on marker (drawn from the HUD's sheet) stays on the enemy |
| Field of view, draw distance, fog, the skies | `qol` | the field of view drawn wider at 150%, far plane and fog 4x with far view and back (telemetry `view`), and with far view the rampart's sky (5.7) with no straight seam and 9.12's clouds with their fine detail (`screen.seams`, `screen.edges`): their pictures wrapped, not smeared; and the rampart's glow cards, which the game's own view never shows, held back (telemetry `wide.sky.cards`) |
| Inverted camera | `qol` | the right stick turns the camera the other way |
| Rumble on both motors | `qol`, `vpad` | the game's requests in a fight (`CV_RUMBLE_TRACE`), and both motors reaching an SDL controller |
| Hector's sprint | `sprint`, `sprint-ps2`, `sprint-eu`, `sprint-keys` (*Linux*) | his walk at 18.519 a frame (telemetry `player.speed`); L3 clicked while walking: 1.5 times it, his walk's motion at that pace too (a motion slot's 256 a frame made 384), an attack meanwhile at its own, kept through a turn, ended by letting go of the stick; 2x; off; L3 + R3 the menu and no sprint; at the Executioner of 5.2, L3 locks on to nothing while Lock-On ON/OFF does; by a controller as SDL reads one, in both button layouts and both releases; Left Shift on the keyboard |
| PS2 stick response | `stick` | a small tilt walks on PS2 and not on Xbox; a tilt near right walks straight right on Xbox only |
| PS2 button layout | `buttons-xbox`, `buttons-ps2`, `buttons-live`, `vpad-ps2` | the icons (asked of the judge), and changed while the game runs on its Controller Config page, PS2 and back; a controller's shoulders and triggers swapped |
| Controllers (SDL) | `vpad`, `vpad-ps2` | a virtual controller read as a real one: buttons, sticks, the settings menu by both sticks, unplugged and plugged in again, rumble |
| Keyboard play | `keyboard` (*Linux*) | each key as its controller button, WASD and the arrows as the sticks, Enter and the pause menu |
| HD texture packs | `texpack`, `texpack-off`, `texpack-1024`, `filters-pack` | the pack indexed (.dds), three in four of the pack's kind of textures drawn with its pictures; none with it off; the size limit taken; sharper with it |
| The pack switched while playing | `texpack-live`, `texpack-live-on` | off and on again in a game started with it, and on in a game started without it: the pictures gone and back (telemetry `texpack`) |
| Material maps, surface depth | `surface` | a pack's materials listed and the courtyard's pictures loaded with their maps; surface depth off and on again moves the floor's stone and puts it back |
| HD movies | `movies-hd` | the opening movie drawn from the pack's video (telemetry `movies`), the game's own with HD movies switched off while it plays and the pack's again on; Original and Fit leave the screen's side bands black, Fill reaches them |
| Anti-aliasing and upscaling, AMD FSR | `aa` (*Windows*) | FSR on the 3D (its signed libraries loaded), off and on while playing, Performance drawing half the size and softer, native again |
| NVIDIA DLSS | `aa-dlss` (*Windows*, a GeForce RTX) | DLSS (DLAA) at the start, Quality drawing two thirds across |
| Frame generation | `fg` (*Windows*) | FSR 3 generating standing and turning: twice the frames shown, the game at 60, presented without v-sync on a screen whose refresh is not a multiple of 120 |
| The renderer's own TAA (Linux's anti-aliasing) | `taa` | the TAA on the 3D (on Windows by `XBE_TAA=1`), still from frame to frame, off and on, Performance upscaling, the camera turned |
| Ambient occlusion | `ao` | with no anti-aliasing: the stone darker where it meets at High, still from frame to frame, lighter with it off, Low between |
| 16:10 widescreen | `wide-1610` | at the Steam Deck's 1280x800: no black bars, the HUD at the edges, 16:9 putting the bars back while playing, the pause menu to both sides |
| Render scale, anisotropic filtering | `filters-pack`, `filters-nopack` | 50% and 200% soften and sharpen, filtering sharpens, with the pack and without |
| Shaders kept on disk | `shadercache-cold`, `shadercache-warm` | a second run compiles no shader |
| Frame rate | `fps`, `fps-uncapped` | held at 60; well above it with the cap and v-sync off |
| Graphics card choice | `gpu-1`, `gpu-2`, ... | each card the launcher lists draws the game when named in `gpu` |
| The in-game settings menu (F10, L3 + R3) | `menu`, `vpad`, `display` (*Linux*) | opened by both sticks and by F10; a switch, a drop-down opened and a choice taken, each saved with the launcher's keys kept; Reset to defaults; Close; the FPS counter in the corner |
| The settings menu by controller | `menu-pad` | a controller alone: RB and LB turning its pages both ways round, B closing it, Start closing it without the game's pause menu opening; the D-pad moving to a setting and A switching it on and off; the right stick turning the camera under the menu |
| Display modes | `display` (*Linux*) | borderless and fullscreen fill the display, windowed is back to its size |
| Linux on Wayland | `wayland` (*Linux*) | a new game on a headless Weston through SDL's Wayland video |

## The launcher

| Feature | Tests | What they check |
|---|---|---|
| Seven languages, every page | `launcher`, `install-*` | every letter of every phrase in its fonts (`--check-fonts`); each of the six pages opens with its controls in each language |
| Every setting | `launcher` | the display mode, widescreen, render scale, filtering, the FPS counter and the button layout clicked and saved as the game reads them; Reset to defaults puts them back, the language and the release kept |
| Both releases, the release played | `launcher` | no list with one disc; with both the list beside Play, the European release chosen and saved, Play starting it; each game executable following the setting |
| The HD pack's row and switch | `launcher` | none in the textures folder; three pictures found and in use; switched off |
| Game files, which copies play | `launcher` | each copy's verdict: the USA xiso with its patched header, the European disc, another release's, a file that is no Xbox disc |
| Graphics that run the game | `launcher` | without them (`CV_NO_D3D12=1`) the home page says "No Direct3D 12" (Linux: "No Vulkan") |
| The graphics cards | `gpu-*` | the cards it lists (`--list-gpus`) are the ones the game takes |

## Copies of the game, the packages

| Feature | Tests | What they check |
|---|---|---|
| Any copy of the USA disc | `discs` | an xiso (its certificate patched, put back), the same as an XGD2 whole disc dump, the extracted files |
| The European release by Castlevania | `discs`, `install-eu` | its disc handed to CastlevaniaEU, which plays it |
| Another release refused | `discs` | another release's xbe (a fan translation's): the message box says so; no copy: it asks for the game |
| The release as a player installs it | `install-usa`, `install-eu` | the package alone (no game files in it, every program and library loading from it or the system, glibc 2.35 at most on Linux), the first start building the game code, a new game played |

## Checked by hand

What no test here reaches, to try before a release when it changed:

- A real controller over USB and Bluetooth (an Xbox, a DualSense, a Switch Pro), plugged in while playing, its rumble.
- Fullscreen and borderless on Windows (the tests take the PC's screen only on Linux), with a monitor of another size.
- Pause on focus loss, and its sound stopping with it, the courtyard's fountain too (off in every scripted run, which goes on behind other windows).
- The launcher's file dialog (Add disc image) and the buttons that open the saves, logs and textures folders.
- SmartScreen and a browser's download warning for a new release (the README's steps).
- A dev build's program icon, made from the game's copy (packages have none).
- Linux on a PC's own graphics drivers (the tests run in WSL through Mesa's dozen) and on a Steam Deck.
- HDR on an HDR display (Windows), and NVIDIA DLSS frame generation (a GeForce RTX 40 or 50).
- Frame generation on a 120 Hz screen with v-sync, and on a variable refresh one (G-SYNC, FreeSync).
- The launcher's downloads of AMD's and NVIDIA's files (the tests copy them from `vendor`).

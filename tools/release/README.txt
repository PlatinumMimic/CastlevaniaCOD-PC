Castlevania: Curse of Darkness - native PC port of the original Xbox version
https://github.com/Pazzi7054/CastlevaniaCOD-PC

This download contains no game: no game files and no game code. You need your own copy of
Castlevania: Curse of Darkness for the original Xbox, USA (NTSC-U) release.

GETTING STARTED
  1. Extract this folder anywhere.
  2. Run Castlevania.exe. The assets folder opens.
  3. Put your game in it: its disc image (.iso), or the extracted disc
     (default.xbe with its data, movie and sound folders).
  4. Run Castlevania.exe again. The first start builds the game code from your copy, which
     takes a few minutes and shows its progress; then press Start. After that the game starts
     straight away.

A disc image can be the whole disc (a redump) or its game partition alone (an xiso), and a copy
whose header was patched for a hard disk loader or another region (title, media, region) is
fine: the game puts the original header back.

CastlevaniaLauncher.exe has every setting (display, widescreen, render scale, controls) and
checks your game files. In game, F10 or clicking both sticks in (L3 + R3) opens the settings.

HD TEXTURE PACKS
  Texture packs made for PCSX2 work too: put the pack's replacement pictures (its .png or .dds
  files, folders inside are fine) in the textures folder. The game matches them the way PCSX2 does,
  by the original textures' hashes. The launcher's Graphics page turns the pack on or off and
  sets the largest size pictures load at (2048 by default; Full needs a lot of video memory).

Saves are in hdd\E\UDATA\4b4e002d. Logs, including logs\compile.log from the first start, are in logs.

The compiler folder holds what builds the game code on your PC: the recompiler, and Clang and
LLD from the LLVM project (see compiler\LLVM-LICENSE.txt). It is used again only if the game code
has to be built again (a new release); the page that says the game is ready can delete it.

This is a non-commercial fan project, not affiliated with Konami or Microsoft. Castlevania is a
trademark of Konami Digital Entertainment.

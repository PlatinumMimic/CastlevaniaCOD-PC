Castlevania: Curse of Darkness - native PC port of the original Xbox version
https://github.com/jmafort/CastlevaniaCOD-PC (a fork of https://github.com/Pazzi7054/CastlevaniaCOD-PC)

This download contains no game: no game files, no disc image and no game code. You need your own
copy of Castlevania: Curse of Darkness for the original Xbox, USA (NTSC-U) release, as an .iso or
its extracted files.

WHAT YOU NEED
  Windows 10 (version 1703 or later) or 11, 64-bit; graphics with Direct3D 12 and its driver
  (NVIDIA GeForce 600 series, AMD Radeon HD 7700, Intel HD 4200, from 2012-2013 on, or newer:
  older graphics run Windows 10 but not the game; dxdiag's Display tab shows Driver Model
  WDDM 2.0 or higher where it works); a processor with SSE4.2 (Intel from 2008, AMD from 2011
  on); about 0.5 GB of disk besides your game. Nothing else to install: the Visual C++ runtime and the compiler the
  first start uses are in this folder. No internet connection is needed.

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

HD TEXTURE PACKS (optional)
  Texture packs fans made for the PS2 game in PCSX2 work too: put the pack's replacement pictures
  (its .png or .dds files, folders inside are fine) in a textures folder beside Castlevania.exe.
  The launcher's home page says whether it found a pack and turns it on or off; its Graphics page
  sets the largest size pictures load at (2048 by default; Full needs a lot of video memory).
  Packs, their makers' own work (not included here):
    Panda_Venom and Stan21, HD Remaster:
      https://gbatemp.net/threads/castlevania-curse-of-darkness-usa-hd-remaster.619688/
    VictorKratos, VK Curse of Darkness:
      https://drive.usercontent.google.com/download?id=1rEeBsGLc93pQerT_7j-MwzT-otxhGZl-&authuser=0
    PlatinumMimic (Calm-Consideration41), 4K Texture Packs:
      https://buymeacoffee.com/platinummin/castlevania-ps2-4k-texture-packs

Saves are in hdd\E\UDATA\4b4e002d. Logs, including logs\compile.log from the first start, are in logs.

The compiler folder holds what builds the game code on your PC: the recompiler, and Clang and
LLD from the LLVM project (see compiler\LLVM-LICENSE.txt). It is used again only if the game code
has to be built again (a new release); the page that says the game is ready can delete it.

This is a personal, non-commercial fan project. It has nothing to do with Konami or Microsoft:
it is not affiliated with, endorsed or sponsored by them. Castlevania is a trademark of Konami
Digital Entertainment.

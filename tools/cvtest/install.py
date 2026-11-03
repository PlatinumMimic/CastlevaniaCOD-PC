"""A release installed and played as a player would: what works on a PC with the build tools should work on one with
nothing but Windows.

usage: python -I tools/cvtest/install.py <release.zip> <game> [<scenario.json>] [--keep]
On Linux (from WSL too) the release is the .tar.xz of tools/linux/release.sh, installed into /tmp; each ELF file's
libraries are checked to be in lib beside it or a part of any desktop Linux, and none to want a glibc newer than the
release's build machine has (2.35); the copy is linked into assets with a symbolic link.

<game> is the player's copy: a disc image (.iso), or a folder with the extracted disc. The zip (release.bat's) is
extracted into a new folder outside this repository, in TEMP: the game looks for assets and textures in the folders
above its own, so inside the repository it could find this PC's. The copy is linked into its assets (a hard link,
else copied). Then
  - every program and DLL in the package is checked to load: each DLL it imports (delay loaded ones too) is beside
    it, or a part of Windows; the Visual C++ runtime and the like do not count, they are not on a fresh Windows
  - Castlevania.exe is started as from Explorer, in an environment of only Windows' own variables (no build tools
    on PATH, none of this PC's XBE_ and CV_ settings), its first start builds the game code and Start is pressed
  - the scenario plays (scenarios/newgame.json by default), as cvtest.py plays it
The report is cvtest's, in out/test/install-<scenario>-<time>, the checks and the first start before the steps.
The install is deleted after a run that passed, unless --keep.
"""
import os
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import time
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # cvtest.py beside this (python -I leaves it off)
import cvtest  # noqa: E402

SYSTEM32 = Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32"
# in System32 on this PC only because something installed them: a fresh Windows has none
REDISTRIBUTED = ("msvcp", "vcruntime", "concrt", "vccorlib", "vcomp", "ucrtbased", "msvcr", "mfc", "d3dx", "xinput1_",
                 "xaudio2_0", "xaudio2_1", "xaudio2_2", "xaudio2_3", "xaudio2_4", "xaudio2_5", "xaudio2_6",
                 "xaudio2_7", "x3daudio", "xapofx", "d3dcompiler_3", "d3dcompiler_43", "libgcc", "libstdc++",
                 "libwinpthread", "sdl", "python", "openal")
# the variables a fresh Windows session has, without the ones programs add
WINDOWS_VARS = ("ALLUSERSPROFILE", "APPDATA", "CommonProgramFiles", "CommonProgramFiles(x86)", "CommonProgramW6432",
                "COMPUTERNAME", "ComSpec", "HOMEDRIVE", "HOMEPATH", "LOCALAPPDATA", "LOGONSERVER",
                "NUMBER_OF_PROCESSORS", "OS", "PATHEXT", "PROCESSOR_ARCHITECTURE", "PROCESSOR_IDENTIFIER",
                "PROCESSOR_LEVEL", "PROCESSOR_REVISION", "ProgramData", "ProgramFiles", "ProgramFiles(x86)",
                "ProgramW6432", "PUBLIC", "SystemDrive", "SystemRoot", "TEMP", "TMP", "USERDOMAIN", "USERNAME",
                "USERPROFILE", "windir")


def imports(path):
    """the DLLs a PE file imports, delay loaded ones too"""
    data = path.read_bytes()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        return []
    sections = struct.unpack_from("<H", data, pe + 6)[0]
    optional = pe + 24
    size = struct.unpack_from("<H", data, pe + 20)[0]
    plus = struct.unpack_from("<H", data, optional)[0] == 0x20B
    dirs = optional + (112 if plus else 96)
    table = optional + size

    def at(rva):  # file offset of an RVA
        for i in range(sections):
            s = table + i * 40
            vsize, va, raw_size, raw = struct.unpack_from("<IIII", data, s + 8)
            if va <= rva < va + max(vsize, raw_size):
                return raw + rva - va
        return None

    def name(rva):
        o = at(rva)
        return data[o:data.index(b"\0", o)].decode("ascii", "replace") if o is not None else "?"

    found = []
    for index, entry, field in ((1, 20, 12), (13, 32, 4)):  # the import table, the delay load table
        rva = struct.unpack_from("<I", data, dirs + index * 8)[0]
        o = at(rva) if rva else None
        while o is not None and any(data[o:o + entry]):
            found.append(name(struct.unpack_from("<I", data, o + field)[0]))
            o += entry
    return found


def check_package(pkg):
    """each DLL a program or DLL in the package loads: beside it, or part of Windows"""
    errors, seen = [], 0
    built = {"castlevania.dll", "castlevaniaeu.dll"}  # each release's game code, which its first start builds
    for f in sorted(p for p in pkg.rglob("*") if p.suffix.lower() in (".exe", ".dll")):
        seen += 1
        for dll in imports(f):
            low = dll.lower()
            if low.startswith(("api-ms-win-", "ext-ms-")) or (f.parent / dll).exists() or low in built:
                continue
            if (SYSTEM32 / dll).exists() and not low.startswith(REDISTRIBUTED):
                continue
            errors.append(f"{f.relative_to(pkg)} needs {dll}, which is neither beside it nor part of Windows")
    return {"step": "package", "label": f"package: {seen} programs and DLLs load", "ok": not errors,
            "errors": errors, "warnings": []}


def check_contents(pkg):
    """nothing of the game in the download, and what the first start needs is there"""
    errors = [f"game file in the package: {p.relative_to(pkg)}" for p in pkg.rglob("*")
              if p.suffix.lower() in (".xbe", ".iso", ".cpp", ".obj", ".ico")
              or p.name.lower() in ("castlevania.dll", "castlevaniaeu.dll") or p.name.startswith("x86_recomp")]
    # both releases' executables (the USA one starts the European one for its game) and what builds their code
    for need in ("Castlevania.exe", "CastlevaniaEU.exe", "CastlevaniaLauncher.exe", "compiler/xbe_recomp.exe",
                 "compiler/clang.exe", "compiler/lld-link.exe", "compiler/config/game.toml",
                 "compiler/config/game_eu.toml", "compiler/Castlevania.lib", "compiler/CastlevaniaEU.lib", "assets",
                 "textures"):
        if not (pkg / need).exists():
            errors.append(f"missing from the package: {need}")
    return {"step": "package", "label": "package: no game files, the first start's tools there", "ok": not errors,
            "errors": errors, "warnings": []}


def check_fonts(launcher, env):
    """every letter of the launcher's phrases, in each of its languages, in the fonts it writes them with: its
    --check-fonts (a font without a language's letters shows ? for them, as Russian did on Linux with TeX Gyre Pagella)"""
    r = subprocess.run([str(launcher), "--check-fonts"], capture_output=True, text=True, encoding="utf-8",
                       errors="replace", env=env, timeout=120)
    missing = [line for line in r.stdout.splitlines() if line.startswith("missing ")]
    errors = missing[:12] + ([f"... {len(missing) - 12} more"] if len(missing) > 12 else [])
    if r.returncode != 0 and not missing:
        errors.append(f"--check-fonts ended with {r.returncode}: {(r.stdout + r.stderr).strip()[-300:]}")
    return {"step": "package", "label": "launcher: every phrase's letters in its fonts, in all its languages",
            "ok": r.returncode == 0 and not errors, "errors": errors, "warnings": []}


def put_game(game, assets):
    """the player's copy into assets: hard links, else copies"""
    def place(src, dst):
        try:
            os.link(src, dst)
        except OSError:
            shutil.copy2(src, dst)

    if game.is_dir():
        shutil.copytree(game, assets, dirs_exist_ok=True, copy_function=place)
    else:
        place(game, assets / game.name)




# --- Linux ------------------------------------------------------------------------------------------------------

GLIBC_MAX = (2, 35)  # the release's build machine's (tools/linux/Containerfile)
GLIBCXX_MAX = (3, 4, 30)  # GCC 12's C++ runtime, Ubuntu 22.04's, which LLVM's own clang wants too
# what every desktop Linux has
LINUX_BASE = ("libc.so.6", "libm.so.6", "libdl.so.2", "libpthread.so.0", "librt.so.1", "ld-linux-x86-64.so.2",
              "libvulkan.so.1", "libz.so.1", "libstdc++.so.6", "libgcc_s.so.1")


def elf_needs(path):
    """an ELF file's needed libraries and the glibc versions it asks for; None when it is no ELF file"""
    data = path.read_bytes()
    if data[:4] != b"\x7fELF" or data[4] != 2:
        return None
    shoff, = struct.unpack_from("<Q", data, 0x28)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 0x3A)
    sections = [struct.unpack_from("<IIQQQQIIQQ", data, shoff + i * shentsize) for i in range(shnum)]
    def cstr(off):
        return data[off:data.index(b"\0", off)].decode("ascii", "replace")
    needed, glibc, glibcxx = [], [], []
    for s in sections:
        kind, link, offset, size = s[1], s[6], s[4], s[5]
        strtab = sections[link][4] if link < len(sections) else 0
        if kind == 6:  # SHT_DYNAMIC
            for i in range(0, size, 16):
                tag, val = struct.unpack_from("<qQ", data, offset + i)
                if tag == 1:
                    needed.append(cstr(strtab + val))
        if kind == 0x6FFFFFFE:  # SHT_GNU_verneed
            at = offset
            while True:
                _, cnt, _, aux, nxt = struct.unpack_from("<HHIII", data, at)
                a = at + aux
                for _ in range(cnt):
                    _, _, _, name, anext = struct.unpack_from("<IHHII", data, a)
                    v = cstr(strtab + name)
                    if v.startswith("GLIBC_") and v[6:].replace(".", "").isdigit():
                        glibc.append(tuple(int(x) for x in v[6:].split(".")))
                    if v.startswith("GLIBCXX_") and v[8:].replace(".", "").isdigit():
                        glibcxx.append(tuple(int(x) for x in v[8:].split(".")))
                    a += anext
                if not nxt:
                    break
                at += nxt
    return needed, max(glibc) if glibc else None, max(glibcxx) if glibcxx else None


def check_package_linux(pkg):
    """each library an ELF file of the package needs: in lib beside the executables, or one every Linux has"""
    errors, seen = [], 0
    bundled = {f.name for f in (pkg / "lib").glob("*")}
    for f in sorted(x for x in pkg.rglob("*") if x.is_file() and not x.is_symlink()):
        found = elf_needs(f)
        if found is None:
            continue
        seen += 1
        needed, glibc, glibcxx = found
        for lib in needed:
            if lib in bundled or lib in LINUX_BASE:
                continue
            errors.append(f"{f.relative_to(pkg)} needs {lib}, which is neither in lib nor on every Linux")
        if glibc and glibc > GLIBC_MAX:
            errors.append(f"{f.relative_to(pkg)} wants glibc {'.'.join(map(str, glibc))}, newer than "
                          f"{'.'.join(map(str, GLIBC_MAX))}")
        if glibcxx and glibcxx > GLIBCXX_MAX:
            errors.append(f"{f.relative_to(pkg)} wants libstdc++ {'.'.join(map(str, glibcxx))}, newer than "
                          f"{'.'.join(map(str, GLIBCXX_MAX))}")
        # the loader's own search, the libraries' libraries too (a library's RUNPATH hides the executable's RPATH)
        ldd = subprocess.run(["ldd", str(f)], capture_output=True, text=True, env={"PATH": "/usr/bin:/bin"})
        for line in ldd.stdout.splitlines():
            if "not found" in line:
                errors.append(f"{f.relative_to(pkg)}: {line.strip()}")
    return {"step": "package", "label": f"package: {seen} programs and libraries load, glibc 2.35 and GCC 12's "
                                        f"libstdc++ at most",
            "ok": not errors, "errors": errors, "warnings": []}


def check_contents_linux(pkg):
    errors = [f"game file in the package: {p.relative_to(pkg)}" for p in pkg.rglob("*")
              if p.suffix.lower() in (".xbe", ".iso", ".cpp", ".o") or p.name in ("Castlevania.so", "CastlevaniaEU.so")
              or p.name.startswith("x86_recomp")]
    for need in ("Castlevania", "CastlevaniaEU", "CastlevaniaLauncher", "compiler/xbe_recomp", "compiler/clang",
                 "compiler/ld.lld", "compiler/config/game.toml", "compiler/config/game_eu.toml",
                 "compiler/Castlevania.hooks", "compiler/CastlevaniaEU.hooks", "lib/libvkd3d.so.1",
                 "lib/libvkd3d-shader.so.1", "lib/libvkd3d-utils.so.1", "fonts/MaterialIcons-Regular.ttf",
                 "fonts/P052-Roman.otf", "fonts/P052-Bold.otf", "fonts/P052-BoldItalic.otf", "assets", "textures"):
        if not (pkg / need).exists():
            errors.append(f"missing from the package: {need}")
    return {"step": "package", "label": "package: no game files, the first start's tools there", "ok": not errors,
            "errors": errors, "warnings": []}


def main_linux(release, game, scenario):
    root = Path("/tmp") / f"cvtest-install-{time.strftime('%Y%m%d-%H%M%S')}"
    with tarfile.open(release) as tf:
        tf.extractall(root, filter="tar")
    exes = list(root.rglob("Castlevania"))
    exes = [e for e in exes if e.is_file()]
    if len(exes) != 1:
        print(f"{release} has {len(exes)} Castlevania")
        return None, None
    pkg = exes[0].parent
    print(f"installed into {pkg}")
    checks = [check_contents_linux(pkg), check_package_linux(pkg)]
    if game.is_dir():
        for e in game.iterdir():
            (pkg / "assets" / e.name).symlink_to(e)
    else:
        (pkg / "assets" / game.name).symlink_to(game)
    # a desktop session's own variables: no build tools' or this PC's XBE_ and CV_ settings. The graphics drivers the
    # machine is set up with pass (in WSL, VK_DRIVER_FILES picks Mesa's dozen, Vulkan on the PC's GPU)
    env = {k: os.environ[k] for k in ("HOME", "USER", "LANG", "XDG_RUNTIME_DIR", "PULSE_SERVER", "VK_DRIVER_FILES",
                                      "VK_ICD_FILENAMES", "GALLIUM_DRIVER") if k in os.environ}
    env["PATH"] = "/usr/local/bin:/usr/bin:/bin"
    checks.append(check_fonts(pkg / "CastlevaniaLauncher", env))
    ok = cvtest.run(str(scenario), None, False, install=pkg, env=env, checks=checks)
    return ok, root


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) < 2:
        print(__doc__)
        return 2
    release, game = Path(args[0]).resolve(), Path(args[1]).resolve()
    scenario = Path(args[2]).resolve() if len(args) > 2 else Path(__file__).resolve().parent / "scenarios" / "newgame.json"
    if cvtest.LINUX:
        ok, root = main_linux(release, game, scenario)
        if ok is None:
            return 1
        if ok and "--keep" not in sys.argv:
            shutil.rmtree(root, ignore_errors=True)  # the symbolic link goes, the player's file stays
        else:
            print(f"kept: {root}")
        return 0 if ok else 1
    root = Path(tempfile.gettempdir()).resolve() / f"cvtest-install-{time.strftime('%Y%m%d-%H%M%S')}"
    # the game's search for assets and textures goes up to six folders: none of this PC's on the way
    for up in list(root.parents)[:6]:
        for kind in ("assets", "textures"):
            if (up / kind).is_dir():
                print(f"{up / kind} would be found by the installed game; set TEMP to another folder")
                return 2
    with zipfile.ZipFile(release) as z:
        z.extractall(root)
    exes = list(root.rglob("Castlevania.exe"))
    if len(exes) != 1:
        print(f"{release} has {len(exes)} Castlevania.exe")
        return 1
    pkg = exes[0].parent
    print(f"installed into {pkg}")
    checks = [check_contents(pkg), check_package(pkg)]
    put_game(game, pkg / "assets")
    env = {k: os.environ[k] for k in WINDOWS_VARS if k in os.environ}
    windows = os.environ.get("SystemRoot", r"C:\Windows")
    env["PATH"] = ";".join([str(SYSTEM32), windows, str(SYSTEM32 / "Wbem"), str(SYSTEM32 / "WindowsPowerShell" / "v1.0")])
    checks.append(check_fonts(pkg / "CastlevaniaLauncher.exe", env))
    ok = cvtest.run(str(scenario), None, False, install=pkg, env=env, checks=checks)
    if ok and "--keep" not in sys.argv:
        shutil.rmtree(root, ignore_errors=True)  # the game's hard link goes, the player's file stays
    else:
        print(f"kept: {pkg}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

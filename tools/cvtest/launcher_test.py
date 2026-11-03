"""The launcher and the game executables with the game files of more than one release, as a player has them.

usage: python -I tools/cvtest/launcher_test.py <release .zip or .tar.xz, or a build folder> <USA disc> <European disc>
       [--other <another release's disc>] [--keep]
A build folder's programs (and its DLLs, fonts and lib folders) are copied into the test's install, without the rest.

The release is installed as install.py installs it (outside this repository, in TEMP or /tmp; on Linux the launcher
and the game on a virtual X display of their own), with each disc linked into its assets, then
  - fonts: every letter of the launcher's phrases, in all its languages, in the fonts it writes them with
  - one disc: the home page has Play and no list of releases
  - both discs: the list is beside Play; choosing the European release saves release = "CastlevaniaEU", and Play
    starts CastlevaniaEU, which finds the European disc
  - Russian: the home page in Russian, a picture of it for the report
  - every page in every language: each opens (--page) with its controls, a picture of each for the report
  - settings: the display mode, widescreen, render scale, filtering, the FPS counter and the button layout clicked
    and saved as the game reads them; Reset to defaults puts them back, the language and the release kept
  - the HD texture pack's row: none in the textures folder, then three pictures found and in use, switched off
  - the Game files page: each copy's verdict (the USA release's patched xiso, the European one, another release's
    with --other, a file that is no Xbox disc)
  - no graphics that run the game (CV_NO_D3D12=1): the home page says so
  - the game executables follow the setting (the SDK's Settings::release): Castlevania with the European release
    chosen hands the game to CastlevaniaEU, CastlevaniaEU with the USA one chosen to Castlevania, and with none chosen
    Castlevania plays the USA disc itself
The launcher is driven with window messages (Windows) or the virtual display's pointer (Linux), never the real mouse;
it says where its controls are through CV_LAUNCHER_STATE. A game started is ended once its log says which disc it
plays, at its first start, before it builds anything. The report is in out/test/launcher-<time>; the install is deleted after a run that passed,
unless --keep.
"""
import json
import os
import shutil
import signal
import subprocess
import sys
import tarfile
import tempfile
import time
import tomllib
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # cvtest.py beside this (python -I leaves it off)
import cvtest  # noqa: E402
import install  # noqa: E402

LINUX = cvtest.LINUX
EXE = cvtest.EXE
USA, EU = "Castlevania", "CastlevaniaEU"


def processes():
    """every process: pid -> (parent pid, executable's file name)"""
    out = {}
    if LINUX:
        for d in Path("/proc").iterdir():
            if d.name.isdigit():
                try:
                    ppid = int((d / "stat").read_text().rsplit(")", 1)[1].split()[1])
                    out[int(d.name)] = (ppid, Path(os.readlink(d / "exe")).name)
                except (OSError, ValueError, IndexError):
                    pass
        return out
    import ctypes
    kernel32 = ctypes.WinDLL("kernel32")
    kernel32.CreateToolhelp32Snapshot.restype = ctypes.c_void_p
    snap = kernel32.CreateToolhelp32Snapshot(0x2, 0)  # TH32CS_SNAPPROCESS
    e = cvtest.PROCESSENTRY32W()
    e.dwSize = ctypes.sizeof(e)
    ok = kernel32.Process32FirstW(ctypes.c_void_p(snap), ctypes.byref(e))
    while ok:
        out[e.th32ProcessID] = (e.th32ParentProcessID, e.szExeFile)
        ok = kernel32.Process32NextW(ctypes.c_void_p(snap), ctypes.byref(e))
    kernel32.CloseHandle(ctypes.c_void_p(snap))
    return out


def family_names(pid):
    """the executables' names of the process and those it started (only this test's: none of the player's games)"""
    procs = processes()
    out, grew = {pid}, True
    while grew:
        grew = False
        for child, (parent, _) in procs.items():
            if parent in out and child not in out:
                out.add(child)
                grew = True
    return {p: procs[p][1] for p in out if p in procs}


def end_family(pid):
    for p in sorted(family_names(pid), reverse=True):
        try:
            if LINUX:
                os.kill(p, signal.SIGKILL)
            else:
                subprocess.run(["taskkill", "/PID", str(p), "/F"], capture_output=True)
        except OSError:
            pass


def wait(what, timeout, poll=0.25):
    end = time.time() + timeout
    while time.time() < end:
        got = what()
        if got:
            return got
        time.sleep(poll)
    return None


class Launcher:
    def __init__(self, pkg, env, out, args=(), more_env=None):
        self.pkg, self.out = pkg, out
        self.state = pkg / "launcher-state.json"
        self.state.unlink(missing_ok=True)
        env = dict(env, CV_LAUNCHER_STATE=str(self.state), **(more_env or {}))
        if LINUX:
            env["DISPLAY"] = cvtest.display()
        self.proc = subprocess.Popen([str(pkg / ("CastlevaniaLauncher" + EXE)), *args], cwd=pkg, env=env,
                                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.win = wait(lambda: cvtest.find_window(self.proc.pid), 30)
        # up once it says where its controls are: the page asked for (home has Play), drawn with them
        pages = ["home", "display", "graphics", "game", "controls", "files"]
        page = pages.index(args[list(args).index("--page") + 1]) if "--page" in args else 0
        self.controls = wait(lambda: self.read().get("page") == page and len(self.read().get("controls", {})) > 3
                             and (page or self.read()["controls"].get("play")) and self.read()["controls"], 30) or {}

    def read(self):
        try:
            return json.loads(self.state.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return {}

    def control(self, name, timeout=5):
        return wait(lambda: self.read().get("controls", {}).get(name), timeout)

    def statuses(self):
        return [k[len("status:"):] for k in self.read().get("controls", {}) if k.startswith("status:")]

    def set_by_click(self, name, done, tries=3):
        """a control clicked until done() (a setting saved), the page it is on first"""
        return self.click_until(name, done, tries)

    def click(self, name):
        """the control clicked by the launcher itself (<state>.click, its TestClick: an ImGui activation), whatever
        window has the focus; once the launcher took it"""
        if not self.control(name):
            return False
        req = Path(str(self.state) + ".click")
        tmp = Path(str(req) + ".tmp")
        tmp.write_text(name + "\n", encoding="utf-8")
        os.replace(tmp, req)
        return bool(wait(lambda: not req.exists(), 5))

    def click_until(self, name, done, tries=3):
        """a click until done(), up to tries times (a list the click opened may close by itself first: on Windows
        ImGui closes one when its window loses the focus)"""
        for _ in range(tries):
            if self.click(name) and wait(done, 4):
                return True
        return False

    def shot(self, name):
        time.sleep(0.8)  # a few frames for the change to draw
        img = cvtest.capture(self.win) if self.win else None
        if img:
            cvtest.write_png(self.out / f"{name}.png", *img)
        return bool(img)

    def close(self):
        end_family(self.proc.pid)
        self.proc.wait(timeout=10)


def settings(pkg):
    try:
        return tomllib.loads((pkg / "castlevania.toml").read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def set_settings(pkg, **keys):
    """the settings file with these keys, the rest kept (toml strings, numbers and flags only)"""
    s = settings(pkg)
    s.update(keys)
    lines = []
    for k, v in s.items():
        lines.append(f"{k} = {json.dumps(v) if isinstance(v, str) else str(v).lower() if isinstance(v, bool) else v}")
    (pkg / "castlevania.toml").write_text("\n".join(lines) + "\n", encoding="utf-8")


def log_text(pkg):
    return "\n".join(p.read_text(encoding="utf-8", errors="replace") for p in (pkg / "logs").glob("*.log"))


DISCS = {}  # each release's disc's file name, as given


def plays_disc(log, exe):
    """the log has exe's release found and played: its disc's line without "another" (the copies of the other release
    are logged too, as another executable's)"""
    return any("Game: default.xbe in" in line and DISCS[exe] in line and "another" not in line
               for line in log.splitlines())


def result(label, errors, warnings=()):
    return {"step": "launcher", "label": label, "ok": not errors, "errors": list(errors), "warnings": list(warnings)}


def game_follows(pkg, env, start, chosen, expect):
    """start (an executable) with the release chosen: the game is handed to expect (or kept, start being expect), whose
    log says it plays its release's disc; ended there, at the first start's building of the game code"""
    set_settings(pkg, release=chosen)
    shutil.rmtree(pkg / "logs", ignore_errors=True)
    if LINUX:
        env = dict(env, DISPLAY=cvtest.display())
    proc = subprocess.Popen([str(pkg / (start + EXE))], cwd=pkg, env=env, stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
    errors = []
    played = wait(lambda: plays_disc(log_text(pkg), expect), 60)
    time.sleep(0.5)  # a handover to the other release would be logged by now
    log = log_text(pkg)
    end_family(proc.pid)
    handed = [line for line in log.splitlines() if "handed to" in line]
    if start == expect and handed:
        errors.append(f"{start} handed the game over: {handed[0].strip()}")
    if start != expect and not any(line.rstrip().endswith((f"handed to {expect}", f"handed to {expect}.exe"))
                                   for line in handed):
        errors.append(f"{start} did not hand the game to {expect}")
    if not played:
        errors.append(f"the log has no disc of {expect} played: {log.strip().splitlines()[-3:] if log.strip() else 'empty'}")
    label = f"{start} with release = \"{chosen}\" plays {expect}"
    return result(label, errors)


def setup(release, kind):
    """the release installed as a player installs it, in TEMP or /tmp (cvtest-<kind>-<time>): its folder there, the
    package's folder in it, the environment of a desktop session alone, and how a copy of the game goes into its
    assets (a hard link or copy on Windows, a symbolic link on Linux); None where the game would find this PC's"""
    root = (Path("/tmp") if LINUX else Path(tempfile.gettempdir()).resolve()) / f"cvtest-{kind}-{time.strftime('%Y%m%d-%H%M%S')}"
    if not LINUX:  # the game looks for assets up to six folders above it: none of this PC's on the way
        for up in list(root.parents)[:6]:
            if (up / "assets").is_dir():
                print(f"{up / 'assets'} would be found by the installed game; set TEMP to another folder")
                return None
    if release.is_dir():  # a build's programs, and what they load
        dest = root / "CastlevaniaCOD-PC"
        (dest / "assets").mkdir(parents=True)
        for f in release.iterdir():
            code = f.name.lower() in ("castlevania.dll", "castlevaniaeu.dll")  # built at the first start, as a player's
            if f.is_file() and not code and (f.suffix.lower() in (".exe", ".dll") or f.name in (USA, EU, "CastlevaniaLauncher")):
                shutil.copy2(f, dest / f.name)
            elif f.is_dir() and f.name in ("fonts", "lib"):
                shutil.copytree(f, dest / f.name)
    elif release.suffix == ".zip":
        with zipfile.ZipFile(release) as z:
            z.extractall(root)
    else:
        with tarfile.open(release) as tf:
            tf.extractall(root, filter="tar")
    pkg = next(p.parent for p in root.rglob("CastlevaniaLauncher" + EXE) if p.is_file())
    print(f"installed into {pkg}")
    (pkg / "assets").mkdir(exist_ok=True)
    if LINUX:
        env = {k: os.environ[k] for k in ("HOME", "USER", "LANG", "XDG_RUNTIME_DIR", "VK_DRIVER_FILES",
                                          "VK_ICD_FILENAMES", "GALLIUM_DRIVER") if k in os.environ}
        env["PATH"] = "/usr/local/bin:/usr/bin:/bin"

        def link(disc):
            (pkg / "assets" / disc.name).symlink_to(disc)
    else:
        env = {k: os.environ[k] for k in install.WINDOWS_VARS if k in os.environ}
        windows = os.environ.get("SystemRoot", r"C:\Windows")
        env["PATH"] = ";".join([str(install.SYSTEM32), windows, str(install.SYSTEM32 / "Wbem")])

        def link(disc):
            install.put_game(disc, pkg / "assets")

    return root, pkg, env, link


def main():
    args = [a for i, a in enumerate(sys.argv[1:], 1) if not a.startswith("--") and sys.argv[i - 1] != "--other"]
    if len(args) != 3:
        print(__doc__)
        return 2
    release, usa, eu = (Path(a).resolve() for a in args)
    DISCS.update({USA: usa.name, EU: eu.name})
    out = cvtest.REPO / "out" / "test" / f"launcher-{time.strftime('%Y%m%d-%H%M%S')}"
    out.mkdir(parents=True)
    installed = setup(release, "launcher")
    if not installed:
        return 2
    root, pkg, env, link = installed
    results = [install.check_fonts(pkg / ("CastlevaniaLauncher" + EXE), env)]

    # one disc: Play, no list
    link(usa)
    # the launcher kept open by Play, so the game it starts stays its child
    set_settings(pkg, display_mode="windowed", width=960, height=540, launcher_keep_open=True)
    la = Launcher(pkg, env, out)
    controls = la.controls
    la.shot("one-disc")
    la.close()
    errors = [] if controls.get("play") else ["the launcher said nothing of its controls (CV_LAUNCHER_STATE)"]
    if "release" in controls:
        errors.append("a list of releases with one disc")
    results.append(result("one disc: Play, no list of releases", errors))

    # both discs: the list, the European release chosen, Play starts it
    link(eu)
    la = Launcher(pkg, env, out)
    errors = []
    if not la.control("release"):
        errors.append("no list of releases beside Play")
    la.shot("both-discs")
    if not errors:
        if not la.click_until("release", lambda: la.control("release." + EU, timeout=0.5)):
            errors.append("the list did not open, or has no European release")
        la.shot("list-open")
    if not errors:
        # the European release clicked in the list, opened again if it closed meanwhile
        chosen = False
        for _ in range(3):
            if not la.control("release." + EU, timeout=0.5):
                la.click_until("release", lambda: la.control("release." + EU, timeout=0.5), tries=1)
            if la.click("release." + EU) and wait(lambda: settings(pkg).get("release") == EU, 4):
                chosen = True
                break
        if not chosen:
            errors.append(f"release is {settings(pkg).get('release')!r} after choosing the European one")
        if la.control("release." + EU, timeout=1):
            errors.append("the list stayed open")
        la.shot("chosen")
    results.append(result("both discs: a list beside Play, the European release chosen and saved", errors))
    errors = []
    shutil.rmtree(pkg / "logs", ignore_errors=True)
    la.click("play")
    names = wait(lambda: (set(family_names(la.proc.pid).values()) & {USA + EXE, EU + EXE}) or None, 20) or set()
    found = wait(lambda: plays_disc(log_text(pkg), EU), 30)
    la.close()
    if names != {EU + EXE}:
        errors.append(f"Play started {sorted(names) or 'nothing'}")
    if not found:
        errors.append("the game's log has no European disc found")
    results.append(result("Play starts CastlevaniaEU, which finds the European disc", errors))

    # Russian
    set_settings(pkg, launcher_language="ru")
    la = Launcher(pkg, env, out)
    ok = la.shot("russian")
    la.close()
    set_settings(pkg, launcher_language="en")
    results.append(result("Russian: the home page (russian.png)", [] if ok else ["no picture of the launcher"]))

    # every page in every language
    pages = ["home", "display", "graphics", "game", "controls", "files"]
    errors = []
    for code in ("en", "de", "es", "fr", "it", "pt-BR", "ru"):
        set_settings(pkg, launcher_language=code)
        for i, page in enumerate(pages):
            la = Launcher(pkg, env, out, ["--page", page])
            shown = wait(lambda: la.read().get("page") == i and len(la.read().get("controls", {})) > 3, 10)
            if not shown:
                errors.append(f"{code}: the {page} page did not open with its controls")
            if not la.shot(f"page-{code}-{page}"):
                errors.append(f"{code}: no picture of the {page} page")
            la.close()
    set_settings(pkg, launcher_language="en")
    results.append(result("every page in every language opens with its controls (page-<language>-<page>.png)", errors))

    # the settings it writes, and Reset to defaults
    set_settings(pkg, release=EU)
    errors = []
    checks = [("display", "mode.1", "display_mode", "borderless"), ("display", "mode.2", "display_mode", "windowed"),
              ("graphics", "wide", "widescreen", False), ("graphics", "scale.0", "render_percent", 50),
              ("graphics", "aniso.3", "anisotropic_filtering", 8), ("game", "fps", "fps_counter", True),
              ("controls", "layout.1", "button_layout", 1)]
    la = None
    for page, control, key, value in checks:
        if not la or la.read().get("page") != pages.index(page):
            if la:
                la.close()
            la = Launcher(pkg, env, out, ["--page", page])
        if not la.set_by_click(control, lambda: settings(pkg).get(key) == value):
            errors.append(f"{control} on {page}: {key} is {settings(pkg).get(key)!r}, not {value!r}")
    if not la.set_by_click("Reset to defaults", lambda: settings(pkg).get("render_percent") == 100):
        errors.append("Reset to defaults did not put render_percent back")
    s = settings(pkg)
    for key, value in (("display_mode", "borderless"), ("widescreen", True), ("render_percent", 100),
                       ("anisotropic_filtering", 0), ("fps_counter", False), ("button_layout", 0),
                       ("launcher_language", "en"), ("release", EU)):
        if s.get(key) != value:
            errors.append(f"after Reset to defaults {key} is {s.get(key)!r}, not {value!r}")
    la.close()
    set_settings(pkg, display_mode="windowed", width=960, height=540, release="")
    results.append(result("settings clicked are saved as the game reads them; Reset to defaults keeps the language and release",
                          errors))

    # the HD texture pack's row
    errors = []
    textures = pkg / "textures"
    textures.mkdir(exist_ok=True)
    for f in textures.glob("*.png"):
        f.unlink()
    la = Launcher(pkg, env, out)
    if not wait(lambda: "None in the textures folder (optional)" in la.statuses(), 5):
        errors.append(f"without pictures: {la.statuses()}")
    la.close()
    for i in range(3):
        (textures / f"{i:016x}-{i:016x}-00000013.png").write_bytes(b"")
    set_settings(pkg, texture_pack=True)
    la = Launcher(pkg, env, out)
    if not wait(lambda: "3 pictures found, in use" in la.statuses(), 5):
        errors.append(f"three pictures: {la.statuses()}")
    la.shot("pack-in-use")
    if not la.set_by_click("homepack", lambda: settings(pkg).get("texture_pack") is False):
        errors.append("the switch did not turn the pack off")
    elif not wait(lambda: "3 pictures found, off: the game's own textures" in la.statuses(), 5):
        errors.append(f"switched off: {la.statuses()}")
    la.close()
    for f in textures.glob("*.png"):
        f.unlink()
    set_settings(pkg, texture_pack=True)
    results.append(result("the HD texture pack's row: none, three found and in use, switched off", errors))

    # the Game files page's verdicts
    errors = []
    junk = pkg / "assets" / "not a disc.iso"
    junk.write_bytes(bytes(4096))
    other = Path(sys.argv[sys.argv.index("--other") + 1]).resolve() if "--other" in sys.argv else None
    if other:
        link(other)
    la = Launcher(pkg, env, out, ["--page", "files"])
    want = [f"{usa.name}  -  USA release (patched header), ready", f"{eu.name}  -  European release, ready",
            f"{junk.name}  -  not an Xbox disc image"]
    if other:
        want.append(f"{other.name}  -  another release, only the USA and European ones work")
    wait(lambda: all(w in la.statuses() for w in want), 10)
    for w in want:
        if w not in la.statuses():
            errors.append(f"no \"{w}\" (it says {la.statuses()})")
    la.shot("files")
    la.close()
    junk.unlink()
    if other:
        (pkg / "assets" / other.name).unlink()
    results.append(result("the Game files page: each copy's verdict (files.png)", errors))

    # no graphics that run the game
    la = Launcher(pkg, env, out, more_env={"CV_NO_D3D12": "1"})
    warning = "No Vulkan" if LINUX else "No Direct3D 12"
    seen = wait(lambda: warning in la.statuses(), 5)
    la.shot("no-d3d12")
    la.close()
    results.append(result(f"CV_NO_D3D12=1: the home page says \"{warning}\"", [] if seen else [f"it says {la.statuses()}"]))

    # the game executables follow the setting
    results.append(game_follows(pkg, env, USA, EU, EU))
    results.append(game_follows(pkg, env, EU, USA, USA))
    results.append(game_follows(pkg, env, USA, "", USA))
    results.append(game_follows(pkg, env, EU, "", EU))

    failed = any(not r["ok"] for r in results)
    cvtest.report(out, "launcher", results, failed)
    for r in results:
        print(f"{'ok  ' if r['ok'] else 'FAIL'} {r['label']}" + "".join(f"\n       {e}" for e in r["errors"]))
    print(f"{'FAILED' if failed else 'passed'}: {out / 'report.md'}")
    if not failed and "--keep" not in sys.argv:
        shutil.rmtree(root, ignore_errors=True)  # the links go, the player's discs stay
    else:
        print(f"kept: {pkg}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

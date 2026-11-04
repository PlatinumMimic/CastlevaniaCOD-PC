r"""The regression suite: every feature of the port, each by the tests that cover it (tools/cvtest/SUITE.md lists
which), run in full before every release, on Windows and on Linux.

usage: python -I tools/cvtest/suite.py [--quick] [--only <test>[,<test>...]] [--from <test>] [--list]
                                       [--no-packages] [--alone] [--linux]
       python -I tools/cvtest/suite.py --check

Runs here (Windows, or Linux and WSL) every test of the list below, one after another: the gameplay scenarios on
this platform's dev build (cvtest.py), then the release packages in out/release as a player installs them
(install.py with each disc, launcher_test.py, discs.py). The scenarios of a session (SESSIONS) play in one game, the
new game they come after once (cvtest.py --session), each still a test with its own result and report; --alone
plays each in a game of its own instead. --quick runs the shorter list marked so, --only some of them, --from the
list from one on; --no-packages leaves the packages out; --linux, run on Windows, then runs the Linux half in WSL.
A test that needs what this PC lacks (a disc, the HD texture pack, a package, the other platform, a folder of
suite.json's, a GeForce RTX) is skipped and said so, never passed.

The summary is out/test/suite-<time>/summary.md, with each test's own report. A full run (no --quick, --only or
--from) that passed with nothing skipped also writes out/test/suite-<platform>.json: the commits it ran on (this
repository's and the SDK's beside it, neither with changes), the packages' SHA-256 and the time. --check says whether
both platforms' are there for the commits checked out now and the packages in out/release, which a release needs.

This PC's own paths are in out/test/suite.json (out/ is not in the repository):
  {"discs": {"usa": "<the USA xiso>", "eu": "<the European disc image>", "other": "<another release's image>",
             "folder": "<the extracted USA disc>"},
   "wsl": {"distro": "Ubuntu", "repo": "/mnt/c/.../CastlevaniaCOD-PC",
           "env": {"CV_BUILD": "<the Linux build>", "VK_DRIVER_FILES": "<a Vulkan driver's manifest>"}},
   "movies": "<an HD movie pack's folder (movies.toml)>",
   "materials": "<an HD texture pack's textures folder with material maps (PlatinumMimic's v4)>",
   "vendor": "<AMD's FSR DLLs and a streamline folder, as the launcher downloads them>"}
On Linux the discs' and folders' paths are the Windows ones, read through /mnt (or given apart as "linux_discs").
"""
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
SDK = REPO.parent / "XboxXBE-Recomp"
LINUX = sys.platform.startswith("linux")
PLATFORM = "linux" if LINUX else "windows"
PY = [sys.executable, "-I"]
OUT = REPO / "out" / "test"
CONFIG = OUT / "suite.json"


def config():
    try:
        return json.loads(CONFIG.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def linux_path(p):
    """a Windows path as WSL reads it (C:\a\b to /mnt/c/a/b)"""
    m = re.match(r"^([A-Za-z]):[\\/](.*)$", p)
    return f"/mnt/{m.group(1).lower()}/{m.group(2).replace(chr(92), '/')}" if m else p


def discs():
    """the copies of the game this PC has, by kind, as paths this platform reads; those missing left out"""
    c = config()
    given = c.get("linux_discs", {}) if LINUX else {}
    out = {}
    for k, v in c.get("discs", {}).items():
        p = Path(given.get(k) or (linux_path(v) if LINUX else v))
        if p.exists():
            out[k] = p
    return out


def folder(key):
    """one of suite.json's folders (movies, materials, vendor) as this platform reads it, or None"""
    v = config().get(key)
    if not v:
        return None
    p = Path(linux_path(v) if LINUX else v)
    return p if p.is_dir() else None


def folder_env(key, variable):
    """the environment a test of one of those folders plays with: the folder named in variable"""
    f = folder(key)
    return {variable: str(f)} if f else {}


def version():
    m = re.search(r"project\(castlevania VERSION ([\d.]+)", (REPO / "CMakeLists.txt").read_text(encoding="utf-8"))
    return m.group(1) if m else "0"


def package():
    name = f"CastlevaniaCOD-PC-{version()}" + ("-linux.tar.xz" if LINUX else ".zip")
    p = REPO / "out" / "release" / name
    return p if p.exists() else None


def dev_build():
    return Path(os.environ.get("CV_BUILD", REPO.parent / "build")) if LINUX else REPO / "out" / "build" / "release"


def gpus():
    """the graphics cards the launcher offers (--list-gpus) that the game can draw with"""
    exe = dev_build() / ("CastlevaniaLauncher" + ("" if LINUX else ".exe"))
    try:
        r = subprocess.run([str(exe), "--list-gpus"], capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.TimeoutExpired):
        return []
    return [l.split(" ", 1)[1] for l in r.stdout.splitlines() if l.startswith("d3d12 ") and "(software)" not in l]


def scenario(name, *args, test=None, quick=False, needs=(), before=None, env=None, platforms=("windows", "linux")):
    """a cvtest run of scenarios/<name>.json; its --set settings and --release also apart, for a session"""
    sets, release = {}, None
    for flag, value in zip(args, args[1:]):
        if flag == "--set":
            k, v = value.split("=", 1)
            try:
                sets[k] = json.loads(v)
            except ValueError:
                sets[k] = v
        elif flag == "--release":
            release = value
    path = str(HERE / "scenarios" / f"{name}.json")
    return {"name": test or name, "cmd": [str(HERE / "cvtest.py"), path, *args], "scenario": path, "set": sets,
            "release": release, "quick": quick, "needs": list(needs), "before": before, "env": env or {},
            "platforms": platforms}


# tests played one after another in one game (cvtest.py --session): the new game they come after once, then each from
# the courtyard where it starts Hector, each with its own report. Each in the order played: those that need the game
# as a new game leaves it first (the texture census of the first courtyard, the first save point's boxes), the
# settings menu, whose Reset to defaults leaves every setting the game's own, last. A session's tests share its
# settings read at the start (the texture pack, the window size of filters and qol); --alone plays every test alone.
# lockon and widescreen never share a game: the first-save route kills the Executioner lockon locks on to for good,
# and once lockon has let him out of his wall that route is stuck in his corridor
SESSIONS = {
    "usa": ["newgame", "texpack", "texpack-live", "fps", "fps-uncapped", "stick", "widescreen", "route", "moai-off", "moai-on", "bot",
            "buttons-xbox", "buttons-live", "shadow", "warp", "rooms", "keyboard", "menu"],
    "wide": ["filters-pack", "qol", "ao", "wide-1610", "lockon"],
    "eu": ["newgame-eu", "widescreen-eu", "rooms-eu"],
    "pack-off": ["filters-nopack", "texpack-off", "texpack-live-on"],
}


def empty_shader_cache():
    shutil.rmtree(OUT / "shader-cache" / "suite", ignore_errors=True)


def tests():
    """the suite, in the order it runs: the dev build's scenarios, then the packages"""
    eu = ["--release", "eu"]
    t = [
        scenario("newgame", quick=True),
        scenario("newgame", *eu, test="newgame-eu", quick=True),
        scenario("warp"),
        scenario("rooms", quick=True),
        scenario("rooms", *eu, test="rooms-eu"),
        scenario("bot"),
        scenario("route"),
        scenario("first-save"),
        # a save made by one release loads in the other, both ways
        scenario("keep-save", test="save-usa"),
        scenario("load-save", *eu, test="load-save-eu"),
        scenario("keep-save", *eu, test="save-eu"),
        scenario("load-save", test="load-save-usa"),
        scenario("trevor"),
        scenario("crazy"),
        scenario("moai", "--set", "lament_save=true", test="moai-on"),
        scenario("moai", test="moai-off"),
        scenario("buttons", test="buttons-xbox"),
        scenario("buttons", "--set", "button_layout=1", test="buttons-ps2", needs=["textures"]),
        scenario("buttons-live", needs=["textures"]),  # the layout changed while the game runs
        scenario("filters", test="filters-pack", needs=["textures"]),
        scenario("filters", "--set", "texture_pack=false", test="filters-nopack"),
        scenario("widescreen", quick=True),
        scenario("widescreen", *eu, test="widescreen-eu"),
        scenario("qol"),
        scenario("lockon", quick=True),
        scenario("lockon", *eu, test="lockon-eu"),
        scenario("shadow", quick=True),
        scenario("texpack", needs=["textures"]),
        scenario("texpack", "--set", "texture_pack=false", test="texpack-off"),
        scenario("texpack", "--set", "texture_pack_size=1024", test="texpack-1024", needs=["textures"]),
        # the pack switched off and on while the game runs, and on in a game started without it
        scenario("texpack-live", needs=["textures"]),
        scenario("texpack-live-on", "--set", "texture_pack=false", needs=["textures"]),
        scenario("shadercache-cold", before=empty_shader_cache),
        scenario("shadercache-warm"),
        scenario("stick"),
        scenario("vpad", quick=True),
        scenario("vpad", "--set", "button_layout=1", test="vpad-ps2"),
        scenario("menu", quick=True),
        scenario("menu-pad"),
        # Hector's sprint, by a controller in both button layouts and both releases, and by the keyboard (Linux)
        scenario("sprint", quick=True),
        scenario("sprint", "--set", "button_layout=1", test="sprint-ps2"),
        scenario("sprint", *eu, test="sprint-eu"),
        scenario("sprint-keys", platforms=["linux"]),
        # 1.8.0: ambient occlusion with no anti-aliasing, the renderer's own TAA, 16:10 at the Steam Deck's size, an HD
        # movie pack, a texture pack's material maps; anti-aliasing, upscaling and frame generation with AMD's and
        # NVIDIA's files (Windows)
        scenario("ao"),
        scenario("taa"),  # Linux's anti-aliasing, on Windows by XBE_TAA
        scenario("wide-1610", quick=True),
        scenario("movies-hd", needs=["movies"], env=folder_env("movies", "CV_MOVIES")),
        scenario("surface", needs=["materials"], env=folder_env("materials", "CV_TEXTURES")),
        scenario("aa", needs=["vendor"], env=folder_env("vendor", "CV_VENDOR_DIR"), platforms=["windows"]),
        scenario("aa-dlss", needs=["vendor", "rtx"], env=folder_env("vendor", "CV_VENDOR_DIR"), platforms=["windows"]),
        scenario("fg", needs=["vendor"], env=folder_env("vendor", "CV_VENDOR_DIR"), platforms=["windows"]),
        # Linux only: on Windows they would take the PC's own screen and keyboard
        scenario("display", env={"CV_WAYLAND": "1"}, platforms=["linux"]),
        scenario("keyboard", platforms=["linux"]),
        scenario("wayland", env={"CV_WAYLAND": "1"}, platforms=["linux"]),
        scenario("fps"),
        scenario("fps", "--set", "fps_cap=0", "--set", "vsync=false", test="fps-uncapped"),
        scenario("language", "--set", "game_language=3", test="language-usa"),
    ]
    for n, lang in ((1, "en"), (3, "de"), (4, "fr"), (5, "es"), (6, "it")):
        t.append(scenario("language", *eu, "--set", f"game_language={n}", test=f"language-eu-{lang}", quick=n == 4))
    for i, g in enumerate(gpus()):
        t.append(scenario("gpu", "--set", f"gpu={g}", test=f"gpu-{i + 1}"))
    d = discs()
    pkg = package()
    pkgs = str(pkg) if pkg else "<package>"
    t += [
        {"name": "install-usa", "cmd": [str(HERE / "install.py"), pkgs, str(d.get("usa", ""))], "quick": True,
         "needs": ["package", "usa"]},
        {"name": "install-eu", "cmd": [str(HERE / "install.py"), pkgs, str(d.get("eu", ""))], "needs": ["package", "eu"]},
        {"name": "launcher", "cmd": [str(HERE / "launcher_test.py"), pkgs, str(d.get("usa", "")), str(d.get("eu", ""))]
         + (["--other", str(d["other"])] if "other" in d else []),
         "quick": True, "needs": ["package", "usa", "eu"]},
        {"name": "discs", "cmd": [str(HERE / "discs.py"), pkgs] + sum(([f"--{k}", str(v)] for k, v in d.items()), []),
         "needs": ["package", "usa"]},
    ]
    for x in t:
        x.setdefault("before", None)
        x.setdefault("env", {})
        x.setdefault("platforms", ("windows", "linux"))
        x.setdefault("quick", False)
    return [x for x in t if PLATFORM in x["platforms"]]


def lacks(test):
    """what this PC lacks for the test, or None"""
    d = discs()
    for need in test["needs"]:
        if need == "package" and not package():
            return f"no {('-linux.tar.xz' if LINUX else '.zip')} of {version()} in out/release"
        if need == "textures" and not any((REPO / "textures").rglob("*.dds")) and not any((REPO / "textures").rglob("*.png")):
            return "no HD texture pack in textures"
        if need in ("usa", "eu", "other", "folder") and need not in d:
            return f"no {need} disc in {CONFIG}"
        if need in ("movies", "materials", "vendor") and not folder(need):
            return f"no {need} folder in {CONFIG}"
        if need == "rtx" and not any("GeForce RTX" in g for g in gpus()):
            return "no GeForce RTX for NVIDIA DLSS"
    return None


def run_test(test, folder):
    """the test's result: passed, failed or skipped, why, its seconds and its report"""
    missing = lacks(test)
    if missing:
        return {"result": "skipped", "why": missing, "seconds": 0}
    if test["before"]:
        test["before"]()
    log = folder / f"{test['name']}.log"
    started = time.time()
    with open(log, "w", encoding="utf-8", errors="replace") as f:
        code = subprocess.run(PY + test["cmd"], stdout=f, stderr=subprocess.STDOUT, cwd=REPO,
                              env=dict(os.environ, **test["env"])).returncode
    text = log.read_text(encoding="utf-8", errors="replace")
    seconds = round(time.time() - started)
    report = next((m.group(1).strip() for m in re.finditer(r"(?m)^(?:\S+: |passed: |FAILED: )(\S*out[\\/]test[\\/]\S+)", text)), "")
    if re.search(r"(?m)^\S+: skipped", text):
        return {"result": "skipped", "why": text.strip().splitlines()[-1], "seconds": seconds, "log": str(log)}
    last = [l for l in text.splitlines() if l.strip()][-3:]
    return {"result": "passed" if code == 0 else "failed", "why": "" if code == 0 else " / ".join(last)[-300:],
            "seconds": seconds, "log": str(log), "report": report}


def run_session(name, members, folder):
    """a session's tests in one cvtest run (one alone runs alone); each's result as run_test gives it"""
    out, playing = {}, []
    for t in members:
        missing = lacks(t)
        if missing:
            out[t["name"]] = {"result": "skipped", "why": missing, "seconds": 0}
        else:
            playing.append(t)
    if len(playing) == 1:
        out[playing[0]["name"]] = run_test(playing[0], folder)
    elif playing:
        releases = {t["release"] for t in playing}
        assert len(releases) == 1, f"session {name}: tests of both releases"
        spec, got = folder / f"session-{name}.json", folder / f"session-{name}-results.json"
        spec.write_text(json.dumps({"name": name, "release": releases.pop(), "parts": [
            {"name": t["name"], "scenario": t["scenario"], "set": t["set"]} for t in playing]}, indent=1), encoding="utf-8")
        env = {}
        for t in playing:
            env.update(t["env"])
            if t["before"]:
                t["before"]()
        log = folder / f"session-{name}.log"
        with open(log, "w", encoding="utf-8", errors="replace") as f:
            code = subprocess.run(PY + [str(HERE / "cvtest.py"), "--session", str(spec), "--results", str(got)],
                                  stdout=f, stderr=subprocess.STDOUT, cwd=REPO, env=dict(os.environ, **env)).returncode
        try:
            parts = json.loads(got.read_text(encoding="utf-8"))["parts"]
        except (OSError, ValueError, KeyError):
            parts = {}
        last = " / ".join([l for l in log.read_text(encoding="utf-8", errors="replace").splitlines() if l.strip()][-3:])
        for t in playing:
            r = parts.get(t["name"])
            out[t["name"]] = dict(r, log=str(log)) if r else {
                "result": "failed", "why": f"the session ended before it (code {code}): {last[-300:]}", "seconds": 0,
                "log": str(log)}
    return {t["name"]: dict(out[t["name"]], played=f"session {name}" if len(playing) > 1 else "alone")
            for t in members}


def plan(chosen, alone):
    """the chosen tests as they run: [(session name or None, [tests])], a session where its first chosen test is"""
    units, placed = [], set()
    for t in chosen:
        home = None if alone else next((s for s, names in SESSIONS.items() if t["name"] in names), None)
        if home is None:
            units.append((None, [t]))
        elif home not in placed:
            placed.add(home)
            names = SESSIONS[home]
            units.append((home, sorted((c for c in chosen if c["name"] in names), key=lambda c: names.index(c["name"]))))
    return units


def git(path, *args):
    r = subprocess.run(["git", "-C", str(path), *args], capture_output=True, text=True)
    return r.stdout.strip()


def commits():
    return {"port": git(REPO, "rev-parse", "HEAD"), "port_changes": bool(git(REPO, "status", "--porcelain", "-uno")),
            "sdk": git(SDK, "rev-parse", "HEAD"), "sdk_changes": bool(git(SDK, "status", "--porcelain", "-uno"))}


def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        while chunk := f.read(1 << 20):
            h.update(chunk)
    return h.hexdigest()


def check():
    """whether both platforms passed the full suite on the commits checked out, with the packages in out/release"""
    now = commits()
    ok = True
    if now["port_changes"] or now["sdk_changes"]:
        print("changes not committed: the suite has not run on them")
        ok = False
    rel = REPO / "out" / "release"
    for platform, name in (("windows", f"CastlevaniaCOD-PC-{version()}.zip"),
                           ("linux", f"CastlevaniaCOD-PC-{version()}-linux.tar.xz")):
        stamp = OUT / f"suite-{platform}.json"
        try:
            s = json.loads(stamp.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            print(f"{platform}: no full run passed ({stamp})")
            ok = False
            continue
        problems = []
        if s["port"] != now["port"] or s["sdk"] != now["sdk"]:
            problems.append(f"ran on {s['port'][:7]} / SDK {s['sdk'][:7]}, not {now['port'][:7]} / {now['sdk'][:7]}")
        p = rel / name
        if not p.exists():
            problems.append(f"no {name}")
        elif s.get("package") != sha256(p):
            problems.append(f"{name} is not the one it tested")
        print(f"{platform}: {'; '.join(problems) if problems else 'passed on ' + s['when']}")
        ok &= not problems
    print("ready to release" if ok else "not ready to release")
    return 0 if ok else 1


def main():
    args = sys.argv[1:]
    if "--check" in args:
        return check()
    all_tests = tests()
    if "--list" in args:
        for t in all_tests:
            print(f"{t['name']:22s} {'quick ' if t['quick'] else '      '}{lacks(t) or ''}")
        return 0
    chosen = all_tests
    if "--quick" in args:
        chosen = [t for t in chosen if t["quick"]]
    if "--only" in args:
        names = args[args.index("--only") + 1].split(",")
        chosen = [t for t in chosen if t["name"] in names]
    if "--from" in args:
        first = args[args.index("--from") + 1]
        chosen = chosen[next(i for i, t in enumerate(chosen) if t["name"] == first):]
    if "--no-packages" in args:
        chosen = [t for t in chosen if "package" not in t["needs"]]
    full = chosen == all_tests
    folder = OUT / f"suite-{time.strftime('%Y%m%d-%H%M%S')}-{PLATFORM}"
    folder.mkdir(parents=True)
    at = commits()
    results = {}
    started = time.time()
    print(f"suite on {PLATFORM}: {len(chosen)} tests, {folder}")
    i = 0
    for session, members in plan(chosen, "--alone" in args):
        if session and len(members) > 1:
            print(f"  session {session}: {', '.join(t['name'] for t in members)} ...", flush=True)
            got = run_session(session, members, folder)
        else:
            print(f"  [{i + 1}/{len(chosen)}] {members[0]['name']} ...", end="", flush=True)
            got = {members[0]["name"]: dict(run_test(members[0], folder), played="alone")}
        for name, r in got.items():
            i += 1
            results[name] = r
            line = f" {r['result']} ({r['seconds']} s){': ' + r['why'] if r['why'] else ''}"
            print(f"  [{i}/{len(chosen)}] {name} ...{line}" if session and len(members) > 1 else line, flush=True)
    passed = all(r["result"] == "passed" for r in results.values())
    lines = [f"# Regression suite on {PLATFORM}: {'passed' if passed else 'not passed'}", "",
             f"Port {at['port'][:7]}{' (with changes)' if at['port_changes'] else ''}, SDK {at['sdk'][:7]}"
             f"{' (with changes)' if at['sdk_changes'] else ''}, {time.strftime('%Y-%m-%d %H:%M')}, "
             f"{round((time.time() - started) / 60)} minutes.", "",
             "| test | result | seconds | played | why | report |", "|---|---|---|---|---|---|"]
    for name, r in results.items():
        lines.append(f"| {name} | {'**' + r['result'] + '**' if r['result'] == 'failed' else r['result']} | "
                     f"{r['seconds']} | {r.get('played', 'alone')} | {r['why'].replace('|', '/')} | "
                     f"{r.get('report', '')} |")
    (folder / "summary.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    counts = {k: sum(r["result"] == k for r in results.values()) for k in ("passed", "failed", "skipped")}
    print(f"suite on {PLATFORM}: {counts['passed']} passed, {counts['failed']} failed, {counts['skipped']} skipped: "
          f"{folder / 'summary.md'}")
    pkg = package()
    if full and passed and not at["port_changes"] and not at["sdk_changes"] and pkg:
        (OUT / f"suite-{PLATFORM}.json").write_text(json.dumps(dict(at, package=sha256(pkg), when=time.strftime(
            "%Y-%m-%d %H:%M"), results={k: v["result"] for k, v in results.items()}), indent=1), encoding="utf-8")
        print(f"full suite passed: out/test/suite-{PLATFORM}.json")
    elif full and passed:
        print("passed, but with changes not committed or no package: no record for a release")
    code = 0 if passed else 1
    if "--linux" in args and not LINUX:
        code |= linux_half([a for a in args if a != "--linux"])
    return code


def linux_half(args):
    """the same suite in WSL, as this PC's suite.json sets it up"""
    w = config().get("wsl", {})
    if not w.get("repo"):
        print(f"--linux: no \"wsl\" in {CONFIG}")
        return 1
    env = " ".join(f"{k}={v!r}" for k, v in w.get("env", {}).items())
    cmd = f"cd {w['repo']!r} && env {env} python3 -I tools/cvtest/suite.py {' '.join(args)}"
    print("suite on linux, in WSL:")
    return subprocess.run(["wsl.exe", "-d", w.get("distro", "Ubuntu"), "--", "bash", "-lc", cmd]).returncode


if __name__ == "__main__":
    sys.exit(main())

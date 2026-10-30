"""Gameplay tests: runs the built game in a test folder of its own (its own settings and saves, the play folder is not
touched), plays a scenario turn by turn through CV_PAD_LIVE (src/common/keyboard.cpp) and checks every step.

usage: python -I tools/cvtest/cvtest.py <scenario.json> [--build out/build/release] [--keep]
       python -I tools/cvtest/cvtest.py --record <route> [--save <name>]    play, and keep it as routes/<route>.json
       python -I tools/cvtest/cvtest.py --make-route <telemetry.jsonl> <route>

A scenario is JSON:
  {"name": "...",
   "after": "<scenario>.json",            optional: that scenario's steps first (a path beside this one)
   "save": "<name>",                      optional: start from the saves a step kept with save_as
   "settings": {"display_mode": ...},     optional: castlevania.toml lines (windowed 960x540 by default)
   "dump": "<va>:<bytes>,...",            optional: guest memory saved after every command (game/live/mem-<n>-<va>.bin)
   "telemetry": <frames>,                 optional: the state every that many frames, game/telemetry.jsonl
   "steps": [step, ...]}
A step plays one of
  "press": "<entries>"                    keyboard.cpp's syntax, "0:a,500:b:300" or frames "0f:lsu:60f"
  "until": "<expression>"                 plays "poll" (default "30f:wait") again until the expression holds, at most
                                          "timeout" seconds (default 60); "poll_by": [["<expression>", "<entries>"],
                                          ...] plays the first whose expression held after the last poll instead
  "sweep": {"maps": [m, ...]}             warps to every room of those maps (all when left out), from the room
                                          table the game wrote (rooms.json), and keeps a screenshot of each; a room
                                          that fails does not stop the sweep
  "route": "<route>"                      walks routes/<route>.json from its first room: the bot (src/common/bot.cpp)
                                          through each room's points, pressing where the recording pressed and
                                          playing its menus where it played them, out by the way the recording
                                          left; enemies within "guard" (default 600, 0 for none) are fought on the
                                          way and story scenes skipped
and may have
  "expect": ["<expression>", ...]         each must hold after the step
  "shot": "<name>"                        keeps the screenshot as <name>.png
  "save_as": "<name>"                     keeps the game's saves (hdd) for a later scenario's "save"
  "note": "..."                           shown in the report
Expressions are Python over the game state (src/common/telemetry.cpp) after the step: frame, map, room,
player.active, player.pos (x, y, z), player.yaw, player.hp, camera.heading, menu, modal, ...; prev is the state
before the step, screen has mean, std and flat (one colour), step has fps and seconds. A field the game did not
report is None.

Each step also fails when the game exits, a command does not finish in time, or the log has a new error or guest
trap. The report (report.md, results.json, a screenshot per step) goes to out/test/<scenario>-<time>/.

--record starts the game as for play (the play folder's settings, keyboard and controllers; a fresh new game, or
the saves of --save) with its state recorded (CV_TELEMETRY, src/common/telemetry.cpp). Once the game is closed the
recording becomes a route, a leg per room as played, and the game's saves become saves/<route>.
"""
import ctypes
import itertools
import json
import math
import os
import shutil
import struct
import subprocess
import sys
import time
import zlib
from ctypes import wintypes
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SAVES = REPO / "out" / "test" / "saves"
ROUTES = Path(__file__).resolve().parent / "routes"
ROUTE_BUTTONS = ("a", "b", "y")  # presses a route keeps: A jumps; X attacks are the bot's own, with guard
DEFAULT_SETTINGS = {"display_mode": "windowed", "width": 960, "height": 540, "texture_pack": True,
                    "pause_on_focus_loss": False}
FRAME_MS = 17  # a frame at 60 fps, for how long a frame-timed command may take

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
user32.SetProcessDPIAware()


# --- the game window -------------------------------------------------------------------------------------------

def find_window(pid):
    found = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def each(hwnd, _):
        owner = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user32.IsWindowVisible(hwnd):
            found.append(hwnd)
        return True

    user32.EnumWindows(each, 0)
    return found[0] if found else None


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [("biSize", wintypes.DWORD), ("biWidth", wintypes.LONG), ("biHeight", wintypes.LONG),
                ("biPlanes", wintypes.WORD), ("biBitCount", wintypes.WORD), ("biCompression", wintypes.DWORD),
                ("biSizeImage", wintypes.DWORD), ("biXPelsPerMeter", wintypes.LONG),
                ("biYPelsPerMeter", wintypes.LONG), ("biClrUsed", wintypes.DWORD), ("biClrImportant", wintypes.DWORD)]


def capture(hwnd):
    """the client area, drawn by the window itself (works behind other windows): width, height, BGRA rows"""
    r = wintypes.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(r))
    w, h = r.right - r.left, r.bottom - r.top
    if w <= 0 or h <= 0:
        return None
    wdc = user32.GetDC(hwnd)
    dc = gdi32.CreateCompatibleDC(wdc)
    bmp = gdi32.CreateCompatibleBitmap(wdc, w, h)
    old = gdi32.SelectObject(dc, bmp)
    user32.PrintWindow(hwnd, dc, 3)  # PW_CLIENTONLY | PW_RENDERFULLCONTENT: what D3D presented
    info = BITMAPINFOHEADER(ctypes.sizeof(BITMAPINFOHEADER), w, -h, 1, 32, 0, 0, 0, 0, 0, 0)  # top-down
    buf = ctypes.create_string_buffer(w * h * 4)
    gdi32.GetDIBits(dc, bmp, 0, h, buf, ctypes.byref(info), 0)
    gdi32.SelectObject(dc, old)
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(dc)
    user32.ReleaseDC(hwnd, wdc)
    return w, h, buf.raw


def write_png(path, w, h, bgra):
    rgb = bytearray(w * h * 3)
    rgb[0::3], rgb[1::3], rgb[2::3] = bgra[2::4], bgra[1::4], bgra[0::4]
    rows = b"".join(b"\0" + bytes(rgb[y * w * 3:(y + 1) * w * 3]) for y in range(h))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    Path(path).write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                           + chunk(b"IDAT", zlib.compress(rows, 6)) + chunk(b"IEND", b""))


def screen_stats(w, h, bgra):
    """mean and spread of the brightness over about 20,000 pixels; flat is one colour (black, white, a fade)"""
    step = max(1, (w * h) // 20000)
    lum = [(bgra[i + 2] * 299 + bgra[i + 1] * 587 + bgra[i] * 114) / 1000 for i in range(0, w * h * 4, step * 4)]
    mean = sum(lum) / len(lum)
    std = (sum((v - mean) ** 2 for v in lum) / len(lum)) ** 0.5
    return {"mean": round(mean, 1), "std": round(std, 1), "flat": std < 3}


# --- expressions over the state ---------------------------------------------------------------------------------

class View:
    """a JSON object as attributes; a missing one is None"""

    def __init__(self, d):
        self._d = d or {}

    def __getattr__(self, k):
        return view(self._d.get(k))

    def __repr__(self):
        return json.dumps(self._d)


def view(v):
    """objects as Views, also inside lists, so `enemies[0].dist` reads"""
    if isinstance(v, dict):
        return View(v)
    if isinstance(v, list):
        return [view(x) for x in v]
    return v


def holds(expr, state, prev, screen, step):
    names = {k: view(v) for k, v in (state or {}).items()}
    names.update(prev=View(prev), screen=View(screen), step=View(step), abs=abs, min=min, max=max, round=round,
                 len=len, any=any, all=all)
    try:
        return bool(eval(expr, {"__builtins__": {}}, names)), None
    except Exception as e:  # a field the game did not report, usually
        return False, f"{type(e).__name__}: {e}"


# --- the run ------------------------------------------------------------------------------------------------------

def command_ms(entries):
    """how long a command plays, in ms, frames counted at 60 fps"""
    end = 0
    for e in entries.replace("\n", ",").split(","):
        f = e.strip().split(":")
        if len(f) < 2 or not f[0]:
            continue
        k = FRAME_MS if f[0].endswith("f") else 1
        hold = int(f[2].rstrip("f")) * k if len(f) > 2 and f[2] else (9 * FRAME_MS if k > 1 else 150)
        end = max(end, int(f[0].rstrip("f")) * k + hold)
    return end


class Game:
    def __init__(self, build, folder, scenario):
        self.folder = folder
        folder.mkdir(parents=True)
        for f in ("Castlevania.exe", "Castlevania.dll"):
            shutil.copy2(build / f, folder / f)  # dates kept: the exe takes a DLL that is not older than it
        settings = dict(DEFAULT_SETTINGS, **scenario.get("settings", {}))
        lines = [f"{k} = {json.dumps(v) if isinstance(v, str) else str(v).lower()}" for k, v in settings.items()]
        (folder / "castlevania.toml").write_text("\n".join(lines) + "\n")
        if scenario.get("save"):
            shutil.copytree(SAVES / scenario["save"], folder / "hdd")
        self.live = folder / "live"
        self.live.mkdir()
        if scenario.get("dump"):  # "<va>:<bytes>,...": guest memory saved after each command, mem-<n>-<va>.bin
            (self.live / "dump.txt").write_text(scenario["dump"] + "\n")
        self.log = folder / "logs" / "castlevania.log"
        self.log_pos = 0
        env = dict(os.environ, CV_PAD_LIVE=str(self.live))
        env.pop("CV_PAD_SCRIPT", None)
        if scenario.get("telemetry"):  # the state every n frames, as a recording would have it
            env.update(CV_TELEMETRY=str(folder / "telemetry.jsonl"), CV_TELEMETRY_EVERY=str(scenario["telemetry"]))
        self.proc = subprocess.Popen([str(folder / "Castlevania.exe")], cwd=folder, env=env)
        self.n = 0
        self.hwnd = None
        for _ in range(300):
            self.hwnd = find_window(self.proc.pid)
            if self.hwnd or self.proc.poll() is not None:
                break
            time.sleep(0.1)

    def alive(self):
        return self.proc.poll() is None

    def play(self, entries):
        """one command; the state after it, its frames per second, or an error"""
        self.n += 1
        tmp = self.live / "cmd.tmp"
        tmp.write_text("\n".join(e.strip() for e in entries.replace("\n", ",").split(",") if e.strip()) + "\n")
        os.replace(tmp, self.live / f"cmd-{self.n}.txt")
        done = self.live / f"done-{self.n}.txt"
        start = time.time()
        deadline = start + command_ms(entries) / 1000 + 30
        while not done.exists():
            if not self.alive():
                return None, None, f"the game exited (code {self.proc.returncode})"
            if time.time() > deadline:
                return None, None, "the command did not finish in time (the game hangs or runs slowly)"
            time.sleep(0.02)
        seconds = time.time() - start
        state = json.loads((self.live / f"state-{self.n}.json").read_text())
        return state, seconds, None

    def new_log_lines(self):
        if not self.log.exists():
            return []
        with open(self.log, "rb") as f:
            f.seek(self.log_pos)
            data = f.read()
            self.log_pos = f.tell()
        return data.decode("utf-8", "replace").splitlines()

    def close(self):
        if self.alive():
            if self.hwnd:
                user32.PostMessageW(self.hwnd, 0x0010, 0, 0)  # WM_CLOSE
            try:
                self.proc.wait(8)
            except subprocess.TimeoutExpired:
                self.proc.kill()


def poll_until(game, expr, timeout, poll, poll_by, state):
    """plays poll (or the poll_by entries whose expression holds) until expr holds; the state, seconds, an error"""
    limit = time.time() + timeout
    seconds, prev = 0, state
    while True:
        entries = poll or "30f:wait"
        for when, then in poll_by:
            if state and holds(when, state, prev, {}, {})[0]:
                entries = then
                break
        state, s, err = game.play(entries)
        seconds += s or 0
        if err or holds(expr, state, prev, {}, {})[0]:
            return state, seconds, err
        if time.time() > limit:
            return state, seconds, f"until {expr!r} did not hold within {timeout} s"


SKIP_SCENES = [["cutscene", "0:start,45f:wait"], ["menu", "0:b,30f:wait"]]


def sweep_steps(game, spec):
    """a warp, a wait and a screenshot for each room in the game's room table"""
    table = json.loads((game.live / "rooms.json").read_text()) if (game.live / "rooms.json").exists() else []
    maps = set(spec.get("maps") or [g["map"] for g in table])
    out = []
    for g in table:
        if g["map"] not in maps:
            continue
        for room in g["rooms"]:
            m = g["map"]
            out.append({"press": f"0:warp={m}.{room},10f:wait:20f", "note": f"warp {m}.{room}", "stop_on_fail": False})
            # a story scene the room starts is skipped with Start, a menu that opens instead is closed
            out.append({"until": f"map == {m} and room == {room} and player.active and not cutscene and not loading "
                                 f"and not menu", "timeout": spec.get("timeout", 30), "poll_by": SKIP_SCENES,
                        "shot": f"room-{m}-{room}", "expect": ["not screen.flat"], "stop_on_fail": False,
                        "sweep_room": f"{m}.{room}"})
    return out


# --- routes -------------------------------------------------------------------------------------------------------

def playing(s):
    p = s.get("player") or {}
    return bool(p.get("active") and p.get("pos") and not s.get("cutscene") and not s.get("loading")
                and not s.get("menu") and not s.get("modal"))


def simplify(pts, eps):
    """Ramer-Douglas-Peucker: the indices of the points that keep the path within eps of it (of each segment, so
    walking back the same way is kept too)"""
    keep, stack = {0, len(pts) - 1}, [(0, len(pts) - 1)]
    while stack:
        a, b = stack.pop()
        (ax, az), (bx, bz) = pts[a], pts[b]
        sx, sz = bx - ax, bz - az
        length2 = sx * sx + sz * sz or 1e-9
        far, at = 0, None
        for i in range(a + 1, b):
            x, z = pts[i]
            t = max(0.0, min(1.0, ((x - ax) * sx + (z - az) * sz) / length2))
            d = math.hypot(x - ax - t * sx, z - az - t * sz)
            if d > far:
                far, at = d, i
        if at is not None and far > eps:
            keep.add(at)
            stack += [(a, at), (at, b)]
    return sorted(keep)


def menu_macros(lines, presses):
    """the menus played through (the pause menu, a save box, ...): {line index of the last play before it: the
    presses from the one that opened it until play went on, as frame-timed entries}, and the lines they took"""
    macros, taken, last_play, i = {}, set(), None, 0
    while i < len(lines):
        s = lines[i]
        if playing(s):
            last_play = i
        elif last_play is not None and (s.get("menu") or s.get("modal")):
            # from the press that opened it, when one came in the half second before (the pause menu is up ~16
            # frames after Start), until play goes on
            near = itertools.takewhile(lambda j: lines[i]["frame"] - lines[j]["frame"] <= 30, range(i, -1, -1))
            start = next((j for j in near if j in presses and j not in taken), i)
            end = next((j for j in range(i, len(lines)) if playing(lines[j])), len(lines))
            f0 = lines[start]["frame"]
            entries = [f"{lines[j]['frame'] - f0}f:{b}:{held}f" for j in range(start, end) for b, held in presses.get(j, [])]
            if entries:
                macros[last_play] = ",".join(entries)
                taken.update(range(start, end))
            i = end
            continue
        i += 1
    return macros, taken


def make_route(lines):
    """recorded play (CV_TELEMETRY lines with "pad") as a route: a leg per room in the order played, each a list of
    points to walk through, [x, z] or [x, z, button, frames] for a press there (A jumps) or [x, z, "macro",
    "<entries>"] for a menu played through there, and where it walked out"""
    held_from, presses = {}, {}  # button: frame pressed; line index: [(button, frames held)]
    for i, s in enumerate(lines):
        for b in filter(None, s.get("pressed", "").split("+")):
            held_from[b] = (i, s["frame"])
        for b in filter(None, s.get("released", "").split("+")):
            if b in held_from:
                at, frame = held_from.pop(b)
                presses.setdefault(at, []).append((b, max(1, s["frame"] - frame)))
    macros, taken = menu_macros(lines, presses)
    legs = []
    for i, s in enumerate(lines):
        if not playing(s):
            continue
        if not legs or (legs[-1]["map"], legs[-1]["room"]) != (s["map"], s["room"]):
            legs.append({"map": s["map"], "room": s["room"], "samples": [], "enemies": 0, "frames": [s["frame"]] * 2})
        leg = legs[-1]
        x, _, z = s["player"]["pos"]
        press = [p for p in presses.get(i, []) if p[0] in ROUTE_BUTTONS and i not in taken]
        if i in macros:
            press = [("macro", macros[i])]
        leg["samples"].append((x, z, press[0] if press else None))
        leg["enemies"] = max(leg["enemies"], len(s.get("enemies") or []))
        leg["frames"][1] = s["frame"]
    route = {"legs": []}
    for n, leg in enumerate(legs):
        samples = leg.pop("samples")
        pts = [(x, z) for x, z, _ in samples]
        forced = [i for i, (_, _, p) in enumerate(samples) if p]
        cuts = sorted({0, len(pts) - 1, *forced})
        idx = sorted({i for a, b in zip(cuts, cuts[1:]) for i in (a + j for j in simplify(pts[a:b + 1], 40))} | set(cuts))
        points, last = [], None
        for i in idx:
            x, z, press = samples[i]
            if not press and last and math.hypot(x - last[0], z - last[1]) < 150 and i != len(pts) - 1:
                continue
            points.append([round(x), round(z)] + ([press[0], press[1]] if press else []))
            last = (x, z)
        leg["points"] = points
        if n + 1 < len(legs) and len(pts) > 1:  # walked out: on past the last point, the way it was going
            ex, ez = pts[-1]
            back = next(((x, z) for x, z in reversed(pts) if math.hypot(ex - x, ez - z) >= 100), pts[0])
            d = math.hypot(ex - back[0], ez - back[1]) or 1
            leg["exit"] = [round(ex + (ex - back[0]) / d * 600), round(ez + (ez - back[1]) / d * 600)]
        route["legs"].append(leg)
    return route


def play_route(game, route, step, prev):
    """walks a route leg by leg: each point a command of its own (the game waits between commands, so Hector walks
    on), a press where the recording pressed, story scenes skipped, enemies within "guard" fought on the way"""
    guard = step.get("guard", 600)
    state, seconds, notes = prev, 0.0, []

    def play(entries):
        nonlocal state, seconds
        st, s, err = game.play(entries)
        seconds += s or 0
        if st:
            state = st
        return err

    legs = route["legs"]
    n = 0
    while n < len(legs):
        leg = legs[n]
        here = f"map == {leg['map']} and room == {leg['room']}"
        st, s, err = poll_until(game, f"{here} and player.active and not cutscene and not loading and not menu", 30,
                                None, SKIP_SCENES, state)
        seconds += s
        state = st or state
        if err:
            return state, seconds, f"leg {n + 1}: not playing in room {leg['map']}.{leg['room']} ({err})", notes
        press, left = None, False
        targets = [(p, 60 if len(p) > 2 else 120) for p in leg["points"]]
        if leg.get("exit"):
            targets.append((leg["exit"], 30))
        for k, (p, radius) in enumerate(targets):
            exit_ = leg.get("exit") and k == len(targets) - 1
            for attempt in range(6):
                pos = (state.get("player") or {}).get("pos") or [p[0], 0, p[1]]
                ms = 5000 + 3 * int(math.hypot(p[0] - pos[0], p[1] - pos[2])) + (20000 if guard else 0)
                entries = [f"0:walk={p[0]}/{p[1]}/{radius}:{ms}"] + ([f"0:guard={guard}"] if guard else [])
                if press and attempt == 0:
                    entries.append(f"0f:{press[0]}:{press[1]}f")
                err = play(",".join(entries))
                if err:
                    return state, seconds, f"leg {n + 1} point {k + 1}: {err}", notes
                result = ((state.get("bot") or {}).get("results") or ["?"])[0]
                if result == "room":
                    left = True
                    break
                if result == "reached" and not exit_:
                    break
                if result == "stuck":
                    return state, seconds, f"leg {n + 1} point {k + 1} {p[:2]}: stuck", notes
                if state.get("cutscene") or state.get("menu"):
                    play("0:start,45f:wait" if state.get("cutscene") else "0:b,30f:wait")
                    notes.append(f"leg {n + 1} point {k + 1}: a {'story scene' if state.get('cutscene') else 'menu'} "
                                 f"on the way")
                    continue
                if exit_:  # reached the point past the exit, or timed out, in the same room
                    return state, seconds, f"leg {n + 1}: the exit at {p} did not lead out of the room", notes
            else:
                return state, seconds, f"leg {n + 1} point {k + 1} {p[:2]}: not reached in 6 tries", notes
            press = p[2:4] if len(p) > 2 and p[2] != "macro" else None
            if left:
                break
            if len(p) > 2 and p[2] == "macro":  # the menu as it was played, standing here, then a moment
                err = play(f"{p[3]},{command_ms(p[3]) // FRAME_MS + 30}f:wait")
                if err:
                    return state, seconds, f"leg {n + 1} point {k + 1}: menu {p[3]!r}: {err}", notes
                if state.get("menu") or state.get("modal"):
                    notes.append(f"leg {n + 1} point {k + 1}: a menu is still up after {p[3]!r}")
        if press and not left:  # a press on the route's last point
            play(f"0f:{press[0]}:{press[1]}f,0f:wait:30f")
        if left:  # where did it go: the next leg's room, or the route lost
            st, s, err = poll_until(game, "player.active and not loading", 30, None, SKIP_SCENES, state)
            seconds += s
            state = st or state
            got = (state.get("map"), state.get("room"))
            nxt = next((j for j in range(n + 1, len(legs)) if (legs[j]["map"], legs[j]["room"]) == got), None)
            if nxt is None:
                return state, seconds, f"leg {n + 1}: walked into room {got[0]}.{got[1]}, not on the route", notes
            if nxt != n + 1:
                notes.append(f"leg {n + 1}: walked into room {got[0]}.{got[1]}, the route's leg {nxt + 1}")
            n = nxt
        else:
            n += 1
    return state, seconds, None, notes


def record(name, build, save):
    """the game as usual (the play folder's settings, the keyboard and controllers), its state recorded; once it is
    closed the recording becomes routes/<name>.json, and its saves saves/<name>"""
    out = REPO / "out" / "test" / f"record-{name}-{time.strftime('%Y%m%d-%H%M%S')}"
    folder = out / "game"
    folder.mkdir(parents=True)
    for f in ("Castlevania.exe", "Castlevania.dll"):
        shutil.copy2(build / f, folder / f)
    if (build / "castlevania.toml").exists():
        shutil.copy2(build / "castlevania.toml", folder / "castlevania.toml")
    if save:
        shutil.copytree(SAVES / save, folder / "hdd")
    tel = out / "telemetry.jsonl"
    env = dict(os.environ, CV_TELEMETRY=str(tel), CV_TELEMETRY_EVERY="6")
    for k in ("CV_PAD_LIVE", "CV_PAD_SCRIPT"):
        env.pop(k, None)
    print(f"recording {name}: play, then close the game ({out})")
    subprocess.run([str(folder / "Castlevania.exe")], cwd=folder, env=env)
    if (folder / "hdd").exists():
        shutil.rmtree(SAVES / name, ignore_errors=True)
        shutil.copytree(folder / "hdd", SAVES / name)
    return save_route(tel, name)


def save_route(telemetry, name):
    lines = [json.loads(l) for l in Path(telemetry).read_text().splitlines() if l.strip()]
    route = make_route(lines)
    if not route["legs"]:
        print("nothing recorded in play")
        return False
    ROUTES.mkdir(exist_ok=True)
    legs = ",\n  ".join(json.dumps(l) for l in route["legs"])  # a leg a line
    (ROUTES / f"{name}.json").write_text(f'{{"recorded": "{time.strftime("%Y-%m-%d %H:%M")}",\n "legs": [\n  {legs}\n ]}}\n')
    rooms = " -> ".join(f"{l['map']}.{l['room']}" for l in route["legs"])
    points = sum(len(l["points"]) for l in route["legs"])
    print(f"routes/{name}.json: {len(route['legs'])} legs, {points} points, {rooms}")
    return True


def load(path):
    """a scenario with the steps of the ones it comes after"""
    scenario = json.loads(Path(path).read_text(encoding="utf-8"))
    if scenario.get("after"):
        first = load(Path(path).parent / scenario["after"])
        scenario["steps"] = first["steps"] + scenario["steps"]
        scenario.setdefault("save", first.get("save"))
    return scenario


def run(scenario_path, build, keep):
    scenario = load(scenario_path)
    name = scenario.get("name", Path(scenario_path).stem)
    out = REPO / "out" / "test" / f"{name}-{time.strftime('%Y%m%d-%H%M%S')}"
    game = Game(build, out / "game", scenario)
    shots = out / "shots"
    shots.mkdir()
    results, prev, failed = [], None, False
    places = {}  # sweep: where each room put Hector and the camera
    print(f"{name}: {out}")
    try:
        if not game.hwnd:
            raise RuntimeError("no game window")
        steps = scenario["steps"]
        for i, step in enumerate(steps, 1):
            if "sweep" in step:
                rooms = sweep_steps(game, step["sweep"])
                steps[i:i] = rooms  # played next
                results.append({"step": i, "label": f"sweep: {len(rooms) // 2} rooms", "ok": bool(rooms),
                                "errors": [] if rooms else ["no rooms.json from the game"], "warnings": []})
                print(f"  {i:3d} sweep of {len(rooms) // 2} rooms")
                continue
            label = step.get("shot") or step.get("note") or step.get("press") or step.get("until")
            r = {"step": i, "label": label, "errors": [], "warnings": []}
            frame0 = prev.get("frame") if prev else None
            if "until" in step:
                state, seconds, err = poll_until(game, step["until"], step.get("timeout", 60), step.get("poll"),
                                                 step.get("poll_by", []), prev)
            elif "route" in step:
                route = json.loads((ROUTES / f"{step['route']}.json").read_text())
                state, seconds, err, notes = play_route(game, route, step, prev)
                r["warnings"] += notes
            else:
                state, seconds, err = game.play(step["press"])
            if err:
                r["errors"].append(err)
            for line in game.new_log_lines():
                if "error: " in line or "guest trap" in line:
                    r["errors"].append("log: " + line.strip())
            screen = {}
            if game.alive():
                img = capture(game.hwnd)
                if img:
                    screen = screen_stats(*img)
                    write_png(shots / f"{i:03d}_{(step.get('shot') or 'step')}.png", *img)
            if state:
                frames = state.get("frame", 0) - (frame0 if frame0 is not None else state.get("frame", 0))
                info = {"seconds": round(seconds or 0, 2),
                        "fps": round(frames / seconds, 1) if seconds and frame0 is not None else None}
                for expr in step.get("expect", []):
                    ok, why = holds(expr, state, prev, screen, info)
                    if not ok:
                        r["errors"].append(f"expect {expr!r} failed" + (f" ({why})" if why else ""))
                r.update(state=state, screen=screen, **info)
                if screen.get("flat"):
                    r["warnings"].append(f"screen is one colour (mean {screen['mean']})")
                # a room ID that lands where another one did: the same room, or an ID the warp falls back from
                p, c = (state.get("player") or {}).get("pos"), (state.get("camera") or {}).get("pos")
                if step.get("sweep_room") and p and c:
                    place = tuple(round(v) for v in p + c)
                    if place in places:
                        r["warnings"].append(f"same place and camera as room {places[place]}")
                    places.setdefault(place, step["sweep_room"])
            if step.get("save_as") and not r["errors"]:
                dest = SAVES / step["save_as"]
                shutil.rmtree(dest, ignore_errors=True)
                shutil.copytree(game.folder / "hdd", dest)
                r["saved"] = step["save_as"]
            r["ok"] = not r["errors"]
            results.append(r)
            print(f"  {i:3d} {'ok  ' if r['ok'] else 'FAIL'} {label}"
                  + "".join(f"\n        {e}" for e in r["errors"] + r["warnings"]))
            if state:
                prev = state
            if not r["ok"]:
                failed = True
                if not game.alive() or step.get("stop_on_fail", True):
                    break
    finally:
        game.close()
        crash = game.folder / "CrashDumps"
        dumps = [p.name for p in crash.glob("*")] if crash.exists() else []
        if dumps:
            failed = True
            results.append({"step": "end", "ok": False, "errors": ["crash dumps: " + ", ".join(dumps)]})
        report(out, name, results, failed)
        if not keep:
            for f in ("Castlevania.exe", "Castlevania.dll"):
                (game.folder / f).unlink(missing_ok=True)
    print(f"{name}: {'FAILED' if failed else 'passed'} ({sum(r['ok'] for r in results)}/{len(results)} steps)")
    return not failed


def report(out, name, results, failed):
    (out / "results.json").write_text(json.dumps(results, indent=1))
    lines = [f"# {name}: {'FAILED' if failed else 'passed'}", "",
             "| step | result | frame | map/room | Hector (x, y, z) | HP | fps | notes |",
             "|---|---|---|---|---|---|---|---|"]
    for r in results:
        s = r.get("state") or {}
        p = s.get("player") or {}
        pos = ", ".join(f"{v:.0f}" for v in p["pos"]) if p.get("pos") else ""
        notes = "; ".join(r.get("errors", []) + r.get("warnings", []))
        lines.append(f"| {r['step']} {r.get('label', '')} | {'ok' if r['ok'] else '**FAIL**'} | {s.get('frame', '')} | "
                     f"{s.get('map', '')}/{s.get('room', '')} | {pos} | {p.get('hp', '')} | {r.get('fps') or ''} | "
                     f"{notes.replace('|', '/')} |")
    (out / "report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    args = sys.argv[1:]
    if not args or (args[0].startswith("-") and args[0] not in ("--record", "--make-route")):
        print(__doc__)
        return 2
    build = REPO / "out" / "build" / "release"
    if "--build" in args:
        build = Path(args[args.index("--build") + 1]).resolve()
    if args[0] == "--record":
        save = args[args.index("--save") + 1] if "--save" in args else None
        return 0 if record(args[1], build, save) else 1
    if args[0] == "--make-route":
        return 0 if save_route(args[1], args[2]) else 1
    return 0 if run(args[0], build, "--keep" in args) else 1


if __name__ == "__main__":
    sys.exit(main())

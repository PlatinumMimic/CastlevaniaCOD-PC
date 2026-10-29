"""Gameplay tests: runs the built game in a test folder of its own (its own settings and saves, the play folder is not
touched), plays a scenario turn by turn through CV_PAD_LIVE (src/common/keyboard.cpp) and checks every step.

usage: python -I tools/cvtest/cvtest.py <scenario.json> [--build out/build/release] [--keep]

A scenario is JSON:
  {"name": "...",
   "save": "<name>",                      optional: start from the saves a step kept with save_as
   "settings": {"display_mode": ...},     optional: castlevania.toml lines (windowed 960x540 by default)
   "steps": [step, ...]}
A step plays one of
  "press": "<entries>"                    keyboard.cpp's syntax, "0:a,500:b:300" or frames "0f:lsu:60f"
  "until": "<expression>"                 plays "poll" (default "30f:wait") again until the expression holds, at most
                                          "timeout" seconds (default 60)
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
"""
import ctypes
import json
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
        v = self._d.get(k)
        return View(v) if isinstance(v, dict) else v

    def __repr__(self):
        return json.dumps(self._d)


def holds(expr, state, prev, screen, step):
    names = {k: (View(v) if isinstance(v, dict) else v) for k, v in (state or {}).items()}
    names.update(prev=View(prev), screen=View(screen), step=View(step), abs=abs, min=min, max=max, round=round)
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
        self.log = folder / "logs" / "castlevania.log"
        self.log_pos = 0
        env = dict(os.environ, CV_PAD_LIVE=str(self.live))
        env.pop("CV_PAD_SCRIPT", None)
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


def run(scenario_path, build, keep):
    scenario = json.loads(Path(scenario_path).read_text(encoding="utf-8"))
    name = scenario.get("name", Path(scenario_path).stem)
    out = REPO / "out" / "test" / f"{name}-{time.strftime('%Y%m%d-%H%M%S')}"
    game = Game(build, out / "game", scenario)
    shots = out / "shots"
    shots.mkdir()
    results, prev, failed = [], None, False
    print(f"{name}: {out}")
    try:
        if not game.hwnd:
            raise RuntimeError("no game window")
        for i, step in enumerate(scenario["steps"], 1):
            label = step.get("shot") or step.get("note") or step.get("press") or step.get("until")
            r = {"step": i, "label": label, "errors": [], "warnings": []}
            frame0 = prev.get("frame") if prev else None
            if "until" in step:
                limit = time.time() + step.get("timeout", 60)
                seconds = 0
                while True:
                    state, s, err = game.play(step.get("poll", "30f:wait"))
                    seconds += s or 0
                    if err or holds(step["until"], state, prev, {}, {})[0]:
                        break
                    if time.time() > limit:
                        err = f"until {step['until']!r} did not hold within {step.get('timeout', 60)} s"
                        break
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
    if not args or args[0].startswith("-"):
        print(__doc__)
        return 2
    build = REPO / "out" / "build" / "release"
    if "--build" in args:
        build = Path(args[args.index("--build") + 1]).resolve()
    return 0 if run(args[0], build, "--keep" in args) else 1


if __name__ == "__main__":
    sys.exit(main())

"""Every kind of copy of the game the port takes, and one it refuses, each alone in assets as a player puts it.

usage: python -I tools/cvtest/discs.py <release .zip or .tar.xz, or a build folder> [--usa <iso>] [--eu <iso>]
                                       [--folder <extracted disc>] [--other <iso of another release>] [--no-xgd2]

The release is installed as launcher_test.py installs it, then for each copy given Castlevania is started (as a
player starts it, whatever the release) with that copy alone in assets, and its log says what it found:
  - usa      the USA release's disc image, an xiso (one whose xbe certificate was patched plays restored)
  - xgd2     the same xiso as a whole disc dump of the XGD2 layout, its game partition at 0xFD90000 (made here, in
             TEMP, on Linux in ~/.cache/cvtest, unless --no-xgd2: a 3 GB file while it runs)
  - eu       the European release's: Castlevania hands the game to CastlevaniaEU, which plays it
  - folder   the extracted disc (default.xbe with its data, movie and sound folders), linked in file by file
  - other    another release's (such as a fan translation's patched xbe): refused, with a message box saying so
  - empty    nothing in assets: a message box asks for the game (Windows; on Linux the log is checked), the folder
             left unopened (CV_NO_OPEN)
Each game is ended once it has said, at its first start, before it builds anything. The report is in
out/test/discs-<time>; the install is deleted after a run that passed, unless --keep.
"""
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # beside this (python -I leaves it off)
import cvtest  # noqa: E402
import launcher_test as lt  # noqa: E402

XGD2_OFFSET = 0xFD90000  # the game partition of an XGD2 disc (the SDK's kernel/disc.cpp probes it)


def make_xgd2(xiso, dest):
    """the xiso as an XGD2 disc's whole dump: its game partition at the offset, the video partition before it left
    as zeros (sparse where the file system allows)"""
    with open(xiso, "rb") as src, open(dest, "wb") as out:
        out.seek(XGD2_OFFSET)
        while chunk := src.read(16 << 20):
            out.write(chunk)
    return dest


def clear(assets):
    for e in assets.iterdir():
        if e.is_symlink() or e.is_file():
            e.unlink()
        else:
            shutil.rmtree(e)


def link_folder(folder, assets):
    """an extracted disc's default.xbe and folders, each file linked (hard links on Windows, symbolic on Linux)"""
    for name in ("default.xbe", "data", "movie", "sound"):
        src = folder / name
        if lt.LINUX:
            (assets / name).symlink_to(src)
        elif src.is_dir():
            shutil.copytree(src, assets / name, copy_function=os.link)
        else:
            os.link(src, assets / name)


def start(pkg, env, timeout, done):
    """Castlevania started; until done(log, dialog) is true, at most timeout seconds: the log, the message box's
    text (Windows), whether done held; every process of it ended after. Without the game the assets folder is not
    opened (CV_NO_OPEN): no window of the desktop's"""
    shutil.rmtree(pkg / "logs", ignore_errors=True)
    env = dict(env, CV_NO_OPEN="1")
    if lt.LINUX:
        env = dict(env, DISPLAY=cvtest.display())
    proc = subprocess.Popen([str(pkg / ("Castlevania" + lt.EXE))], cwd=pkg, env=env, stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
    dialog, log, held = None, "", False
    end = time.time() + timeout
    while time.time() < end:
        log = lt.log_text(pkg)
        if not lt.LINUX:
            for h in cvtest.windows(proc.pid):
                if cvtest.window_class(h) == "#32770":
                    dialog = cvtest.dialog_text(h) or "a message box"
        if done(log, dialog):
            held = True
            break
        if proc.poll() is not None and not lt.family_names(proc.pid):
            break
        time.sleep(0.25)
    time.sleep(0.5)  # a handover would be logged by now
    log = lt.log_text(pkg)
    lt.end_family(proc.pid)
    return log, dialog, held


def game_lines(log):
    return [l.split("] ", 1)[-1] for l in log.splitlines() if "Game: " in l]


def main():
    args = sys.argv[1:]
    if not args or args[0].startswith("--"):
        print(__doc__)
        return 2

    def opt(name):
        return Path(args[args.index(name) + 1]).resolve() if name in args else None

    release = Path(args[0]).resolve()
    out = cvtest.REPO / "out" / "test" / f"discs-{time.strftime('%Y%m%d-%H%M%S')}"
    out.mkdir(parents=True)
    installed = lt.setup(release, "discs")
    if not installed:
        return 2
    root, pkg, env, link = installed
    assets = pkg / "assets"
    results = []

    def case(label, put, done, check, timeout=60):
        clear(assets)
        put()
        log, dialog, held = start(pkg, env, timeout, done)
        errors = check(log, dialog) if held else [f"not seen in {timeout} s: {game_lines(log)[-2:] or dialog or 'no log'}"]
        results.append(lt.result(label, errors))

    usa, eu, folder, other = opt("--usa"), opt("--eu"), opt("--folder"), opt("--other")

    def plays(name):  # the copy found as this executable's release, as it is or with its certificate put back
        return lambda log, dialog: any(name in l and "another" not in l for l in game_lines(log))

    def no_handover(log, dialog):
        return [l for l in game_lines(log) if "handed to" in l][:1]

    if usa:
        case(f"usa: {usa.name}, the USA release's xiso", lambda: link(usa), plays(usa.name), no_handover)
        if "--no-xgd2" not in args:
            # on Linux not in /tmp, which is often in memory: the user's cache, on the disk
            where = Path.home() / ".cache" / "cvtest" if lt.LINUX else root
            where.mkdir(parents=True, exist_ok=True)
            xgd2 = make_xgd2(usa, where / "Castlevania (XGD2 layout).iso")
            case("xgd2: the same as a whole disc dump of the XGD2 layout", lambda: link(xgd2),
                 plays(xgd2.name), no_handover)
            xgd2.unlink()
    if eu:
        case(f"eu: {eu.name}, the European release's, handed to CastlevaniaEU", lambda: link(eu),
             lambda log, dialog: any(eu.name in l and "another" not in l for l in game_lines(log)),
             lambda log, dialog: [] if any(l.rstrip().endswith(("handed to CastlevaniaEU", "handed to CastlevaniaEU.exe"))
                                           for l in game_lines(log)) else ["not handed to CastlevaniaEU"])
    if folder:
        case(f"folder: {folder}, the extracted disc", lambda: link_folder(folder, assets),
             lambda log, dialog: any("default.xbe in" in l and ".iso" not in l and "another" not in l
                                     for l in game_lines(log)), no_handover)
    if other:
        refused = "different release"
        case(f"other: {other.name}, another release, refused", lambda: link(other),
             lambda log, dialog: (dialog and refused in dialog) if not lt.LINUX else
             any(other.name in l and l.rstrip().endswith("another release") for l in game_lines(log)),
             lambda log, dialog: [] if lt.LINUX or refused in (dialog or "") else [f"message box: {dialog}"])
    case("empty: nothing in assets, asked for the game", lambda: None,
         lambda log, dialog: "setup: no game in" in log and (lt.LINUX or bool(dialog)),
         lambda log, dialog: [] if lt.LINUX or "assets folder" in (dialog or "") else [f"message box: {dialog}"],
         timeout=30)

    failed = any(not r["ok"] for r in results)
    cvtest.report(out, "discs", results, failed)
    for r in results:
        print(f"{'ok  ' if r['ok'] else 'FAIL'} {r['label']}" + "".join(f"\n       {e}" for e in r["errors"]))
    print(f"{'FAILED' if failed else 'passed'}: {out / 'report.md'}")
    if not failed and "--keep" not in args:
        shutil.rmtree(root, ignore_errors=True)  # the links go, the player's copies stay
    else:
        print(f"kept: {pkg}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

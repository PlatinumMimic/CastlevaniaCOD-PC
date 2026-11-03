"""A movie pack for the game (src/common/movies.cpp): HD versions of its CG movies, drawn in place of the disc's.

For each of the game's movies (movie\\*.xmv, from the disc or its extracted files) with an HD video of the same name
in the videos folder (st01_00.mp4: the picture without its black bars, the original's frames one for one, 29.97 fps):

  <name>.sig   the original's thumbnails, which the game matches its frames to: 16x12 a frame, each the mean grey
               ((R+G+B)/3) of a 40x40 block of the 640x480 frame, the first frame first
  <name>.mp4   the video as H.264 (--encode, on an NVIDIA card: NVENC), which every Windows decodes; or left where it is
               and named from movies.toml (--in-place), for a try with videos Windows decodes there
  movies.toml  each movie's video, frame count and the rows its picture lies on in the 640x480 frame

  make_movie_pack.py <ffmpeg> <game movie folder> <videos folder> <pack folder> [--encode | --in-place]
                     [--report prepare-report.json] [--cq 22] [name ...]

The picture's rows come from the report the movies were prepared with (their "crop"), else from the movie itself
(where its black bars end), checked against the video's shape.
"""
import json, os, re, subprocess, sys
import numpy as np

W, H = 640, 480
SW, SH = 16, 12
FPS = "30000/1001"


def run(cmd, **kw):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, **kw)


def thumbnails(ff, xmv):
    """every frame's 16x12 grey thumbnail, (frames, 192) uint8"""
    p = run([ff, "-v", "error", "-i", xmv, "-vf", f"scale={SW}:{SH}:flags=area", "-f", "rawvideo", "-pix_fmt", "rgb24",
             "-"])
    a = np.frombuffer(p.stdout, np.uint8)
    n = len(a) // (SW * SH * 3)
    a = a[: n * SW * SH * 3].reshape(n, SW * SH, 3).astype(np.uint16)
    return ((a.sum(2) + 1) // 3).astype(np.uint8)


def video_info(ff, path):
    """frames, width, height, codec of a video's first video stream"""
    p = run([ff, "-hide_banner", "-i", path, "-map", "0:v:0", "-c", "copy", "-f", "null", "-"])
    err = p.stderr.decode("utf-8", "replace")
    m = re.search(r"Stream #\S+: Video: (\w+).*?, (\d{2,5})x(\d{2,5})", err)
    frames = re.findall(r"frame=\s*(\d+)", err)
    if not m or not frames:
        return None
    return int(frames[-1]), int(m.group(2)), int(m.group(3)), m.group(1)


def bars(ff, xmv):
    """the first and last rows of the picture over the whole movie: rows brighter than black in some frame"""
    p = run([ff, "-v", "error", "-i", xmv, "-vf", "format=gray", "-f", "rawvideo", "-"])
    a = np.frombuffer(p.stdout, np.uint8)
    n = len(a) // (W * H)
    rows = a[: n * W * H].reshape(n, H, W).max(axis=(0, 2))
    lit = np.nonzero(rows > 24)[0]
    return (int(lit[0]), int(lit[-1])) if len(lit) else (0, H - 1)


def main():
    args = [a for a in sys.argv[1:]]
    opts = {"--encode": False, "--in-place": False}
    report, cq = None, 22
    rest = []
    i = 0
    while i < len(args):
        a = args[i]
        if a in opts:
            opts[a] = True
        elif a == "--report":
            report = json.load(open(args[i + 1], encoding="utf-8"))
            i += 1
        elif a == "--cq":
            cq = int(args[i + 1])
            i += 1
        else:
            rest.append(a)
        i += 1
    if len(rest) < 4 or opts["--encode"] == opts["--in-place"]:
        sys.exit(__doc__)
    ff, game, videos, pack = rest[:4]
    only = set(rest[4:])
    os.makedirs(pack, exist_ok=True)
    toml = ["# HD movies for Castlevania: Curse of Darkness (src/common/movies.cpp), made by make_movie_pack.py", ""]
    made, missing, bad = [], [], []
    for xmv in sorted(os.listdir(game)):
        if not xmv.lower().endswith(".xmv"):
            continue
        name = xmv[:-4].lower()
        if only and name not in only:
            continue
        src = next((os.path.join(videos, f) for f in os.listdir(videos)
                    if os.path.splitext(f)[0].lower() == name and f.lower().endswith((".mp4", ".mkv", ".mov"))), None)
        if not src:
            missing.append(name)
            continue
        sig = thumbnails(ff, os.path.join(game, xmv))
        info = video_info(ff, src)
        if not info:
            bad.append(f"{name}: {src} does not read")
            continue
        frames, vw, vh, codec = info
        if frames != len(sig):
            bad.append(f"{name}: the video has {frames} frames, the game's movie {len(sig)}")
            continue
        crop = (report or {}).get(name, {}).get("crop")
        if not crop:
            top, bottom = bars(ff, os.path.join(game, xmv))
            crop = [0, top, W, bottom - top + 1]
        # the video's shape against the picture's: the same within a row of the original's
        if abs(vh * W / vw - crop[3]) > 1.5:
            bad.append(f"{name}: the video is {vw}x{vh}, the picture's rows {crop[1]}..{crop[1] + crop[3] - 1} are "
                       f"{W}x{crop[3]}")
            continue
        with open(os.path.join(pack, name + ".sig"), "wb") as f:
            f.write(sig.tobytes())
        if opts["--encode"]:
            out = os.path.join(pack, name + ".mp4")
            # NVENC's H.264 at constant quality, a keyframe each second so the game finds a frame quickly
            p = run([ff, "-v", "error", "-y", "-i", src, "-map", "0:v:0", "-an", "-c:v", "h264_nvenc", "-preset", "p7",
                     "-tune", "hq", "-rc", "vbr", "-cq", str(cq), "-b:v", "0", "-maxrate", "80M", "-bufsize", "160M",
                     "-profile:v", "high", "-pix_fmt", "yuv420p", "-g", "30", "-bf", "2", "-r", FPS,
                     "-movflags", "+faststart", out])
            if p.returncode != 0:
                bad.append(f"{name}: encoding failed: {p.stderr.decode('utf-8', 'replace').strip()[-300:]}")
                continue
            check = video_info(ff, out)
            if not check or check[0] != frames:
                bad.append(f"{name}: the encoded video has {check[0] if check else 'no'} frames, not {frames}")
                continue
            video = name + ".mp4"
        else:
            video = os.path.relpath(src, pack).replace("\\", "/")
        toml += [f"[{name}]", f'video = "{video}"', f"frames = {frames}", f"crop = [{', '.join(map(str, crop))}]", ""]
        made.append(name)
        print(f"{name:9} {frames:5} frames  {vw}x{vh} {codec}  rows {crop[1]}..{crop[1] + crop[3] - 1}", flush=True)
    with open(os.path.join(pack, "movies.toml"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(toml))
    print(f"\n{len(made)} movies in {pack}")
    if missing:
        print("no HD video: " + ", ".join(missing))
    for b in bad:
        print("NOT ADDED " + b)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()

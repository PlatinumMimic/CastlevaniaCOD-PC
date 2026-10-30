"""The judge: asks a model what a screenshot shows, whether it rendered right and whether Hector is in it. Jev
(TypeSafe System One, https://docs.typesafe.ai) answers with probabilities rather than text, in well under a second.

usage: python -I tools/cvtest/judge.py <shot.png> [...] [--model jev-latest] [--env-file <.env>]

The key is TYPESAFE_API_KEY, from the environment or a .env file (--env-file, or TYPESAFE_ENV_FILE). Answers are
{"scene": "<one of SCENES>", "scene_p": {...}, "defect": <0..1>, "hector": <0..1>}.
"""
import base64
import json
import os
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

API = "https://api.typesafe.ai/v1/systemone"

SCENES = {
    "gameplay": "the player controls Hector in a 3D level, the HUD (HP bar) at the top left",
    "pause_menu": "the pause or status menu",
    "message_box": "a text box or dialog over the gameplay",
    "map_screen": "the castle map",
    "title_or_menu": "the title screen or a front-end menu (name entry, save and load)",
    "cutscene": "a story scene or a pre-rendered movie",
    "blank": "a black or one-colour screen, a fade, loading",
}
DEFECT = ("the picture has a rendering defect: garbage or exploded polygons (long slivers across the screen), "
          "corrupted textures (blocky noise), missing textures (flat white or solid areas), wrong colours (red and "
          "blue swapped, say), black or unrendered regions, or an upside-down image. Effects, motion blur, light rays, "
          "sparks, smoke and dark scenes are not defects")
HECTOR = "Hector, the white-haired man in a long dark coat with a red scarf, is visible"

# the same three questions, as Jev takes them; the picture is `screen` in the state
QUESTIONS = {
    "scene": {"type": "choice", "instructions": "What is the game showing in `screen`?", "criteria": SCENES},
    "defect": {"type": "noul", "instructions": f"In `screen`, {DEFECT}."},
    "hector": {"type": "noul", "instructions": f"In `screen`, {HECTOR}."},
}


def key(env_file=None):
    if os.environ.get("TYPESAFE_API_KEY"):
        return os.environ["TYPESAFE_API_KEY"]
    path = env_file or os.environ.get("TYPESAFE_ENV_FILE")
    if path and Path(path).exists():
        for line in Path(path).read_text(encoding="utf-8").splitlines():
            if line.startswith("TYPESAFE_API_KEY="):
                return line.split("=", 1)[1].strip().strip('"')
    raise SystemExit("no TypeSafe key: set TYPESAFE_API_KEY, or give --env-file / TYPESAFE_ENV_FILE")


def jev(png, api_key, model="jev-latest", state=None, tries=4):
    """one screenshot judged: the answers and {"seconds", "tokens", "model"}"""
    ref = "img-3c71e0a9b4d2"  # replaced as plain text wherever it appears: a name nothing else will contain
    body = {"model": model, "state": dict(state or {}, screen=ref), "questions": QUESTIONS,
            "assets": {ref: {"type": "image", "format": "png", "data_base64": base64.b64encode(Path(png).read_bytes()).decode()}}}
    data = json.dumps(body).encode()
    for attempt in range(tries):
        req = urllib.request.Request(API, data=data, method="POST", headers={
            "Authorization": f"Bearer {api_key}", "Content-Type": "application/json",
            "TypeSafe-Danger-Zone-Images": "true"})
        start = time.time()
        try:
            with urllib.request.urlopen(req, timeout=120) as r:
                res = json.load(r)
            break
        except urllib.error.HTTPError as e:
            if e.code in (429, 500, 502, 503, 529) and attempt + 1 < tries:
                time.sleep(2 ** attempt)
                continue
            raise RuntimeError(f"Jev: HTTP {e.code}: {e.read()[:300]!r}") from None
    a = res["answers"]
    answers = {"scene": a["scene"]["choice"], "scene_p": {k: round(v, 3) for k, v in a["scene"]["probabilities"].items()},
               "defect": round(a["defect"]["noul"], 3), "hector": round(a["hector"]["noul"], 3)}
    return answers, {"seconds": round(time.time() - start, 2), "tokens": res.get("usage", {}).get("input_tokens"),
                     "model": res.get("model")}


def main():
    args = sys.argv[1:]
    model = args[args.index("--model") + 1] if "--model" in args else "jev-latest"
    env_file = args[args.index("--env-file") + 1] if "--env-file" in args else None
    shots = [a for i, a in enumerate(args) if not a.startswith("--") and (i == 0 or args[i - 1] not in ("--model", "--env-file"))]
    if not shots:
        print(__doc__)
        return 2
    k = key(env_file)
    for shot in shots:
        answers, info = jev(shot, k, model)
        print(json.dumps({"shot": shot, **answers, **info}))
    return 0


if __name__ == "__main__":
    sys.exit(main())

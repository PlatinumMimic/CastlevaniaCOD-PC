"""Normal and ORM maps for the textures the recomp's census found drawn on the 3D world with an HD picture but no
material: DeepBump's normals from each picture (green turned to the Remaster's direction, strengthened to its maps'
measure, without DeepBump's tilt of whole regions), occlusion from the height those normals give, roughness about the Remaster's 0.8, no metal; a manifest in
the pcsx2-material-replacements-v1 format beside them, each at the Remaster's mixed_environment_props parameters.

  make_materials.py <census.jsonl> <pack replacements> <hand-made manifest.json> <out folder> [workers] [limit]

Needs a clone of https://github.com/HugoTini/DeepBump (GPL-3.0, its deepbump256.onnx) beside this script or named by
DEEPBUMP, and Python with numpy, pillow and onnxruntime-directml (or onnxruntime). The maps it makes are of the
pictures, the game's art as a pack is: they stay out of this repository.
"""
import json, os, sys, time
import multiprocessing as mp
import numpy as np
from PIL import Image

DEEPBUMP = os.environ.get('DEEPBUMP', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'DeepBump'))  # a clone of HugoTini/DeepBump
sys.path.insert(0, DEEPBUMP)
MAX_SIDE = 1024      # the hand-made maps' size
XY_SPREAD = 0.26     # how far the Remaster's normals tilt, on average (standard deviation of x and y)
PARAMS = {"normal_strength": 0.95, "response_strength": 0.72, "specular_strength": 0.32, "ao_strength": 0.24,
          "roughness_bias": 0.03, "metalness_scale": 0.0, "light_direction": [-0.35, -0.45, 0.82],
          "depth_scale": 2048.0}

_sess = None


def session():
    global _sess
    if _sess is None:
        import onnxruntime as ort
        ort.disable_telemetry_events()
        _sess = ort.InferenceSession(os.path.join(DEEPBUMP, 'deepbump256.onnx'),
                                     providers=['DmlExecutionProvider', 'CPUExecutionProvider'])
    return _sess


def deepbump(gray):
    import utils_inference as ui
    img = gray[None].astype(np.float32)
    stride = 256 - 256 // 4
    tiles, pads = ui.tiles_split(img, (256, 256), (stride, stride))
    pred = ui.tiles_infer(tiles, session())
    out = ui.tiles_merge(pred, (stride, stride), (3, img.shape[1], img.shape[2]), pads)
    return ui.normalize(out)  # C,H,W in 0..1, DeepBump's own direction


def blur(a, r):
    # a box blur r wide, twice: near enough a gaussian, numpy only
    k = 2 * r + 1
    for axis in (0, 1):
        for _ in range(2):
            c = np.cumsum(np.pad(a, [(r + 1, r) if i == axis else (0, 0) for i in range(2)], mode='wrap'), axis=axis)
            a = (np.take(c, range(k, c.shape[axis]), axis=axis) - np.take(c, range(0, c.shape[axis] - k), axis=axis)) / k
    return a


def make(job):
    name, src, out = job
    stem = os.path.splitext(name)[0]
    if all(os.path.exists(os.path.join(out, 'maps', stem + k)) for k in ('_normal.png', '_orm.png')):
        return name, None  # made by an earlier run
    try:
        im = Image.open(src).convert('RGBA')
        w, h = im.size
        s = min(1.0, MAX_SIDE / max(w, h))
        if s < 1:
            im = im.resize((max(1, round(w * s)), max(1, round(h * s))), Image.LANCZOS)
        px = np.asarray(im, np.float32) / 255
        rgb, alpha = px[..., :3], px[..., 3]
        gray = rgb.mean(-1)
        db = deepbump(gray)  # 3,H,W
        import module_normals_to_height as nh
        height = nh.apply(db, False, None)[0]
        n = db.transpose(1, 2, 0) * 2 - 1
        n[..., 1] = -n[..., 1]          # the Remaster's green: y down the picture
        # relief only: DeepBump also tilts whole regions, which would light a door's half or a wall's end as a slope;
        # the hand-made maps have none of that. What is left is made about as strong as theirs
        xy = n[..., :2] - np.stack([blur(n[..., 0], 24), blur(n[..., 1], 24)], -1)
        spread = float(xy.std()) + 1e-6
        xy *= min(XY_SPREAD / spread, 2.5)
        n = np.concatenate([xy, np.sqrt(np.clip(1 - (xy ** 2).sum(-1, keepdims=True), 0.0025, 1))], -1)
        n /= np.linalg.norm(n, axis=-1, keepdims=True) + 1e-8
        flat = alpha < 0.05              # nothing drawn there: flat
        n[flat] = (0, 0, 1)
        normal = np.clip((n * 0.5 + 0.5) * 255 + 0.5, 0, 255).astype(np.uint8)
        # occlusion: below its surroundings in the height, darker, about the Remaster's 0.93 on average
        cav = blur(height, 6) - height
        cav = cav / (cav.std() + 1e-6)
        ao = np.clip(1 - 0.07 * np.maximum(cav, 0) ** 1.2, 0.45, 1)
        # roughness: about the Remaster's 0.8, bright parts a little smoother
        lum = gray - blur(gray, 16)
        rough = np.clip(0.8 - 0.25 * lum, 0.6, 0.95)
        ao[flat], rough[flat] = 1, 0.8
        orm = np.stack([ao, rough, np.zeros_like(ao)], -1)
        orm = np.clip(orm * 255 + 0.5, 0, 255).astype(np.uint8)
        Image.fromarray(normal, 'RGB').save(os.path.join(out, 'maps', stem + '_normal.png'), optimize=False)
        Image.fromarray(orm, 'RGB').save(os.path.join(out, 'maps', stem + '_orm.png'), optimize=False)
        return name, None
    except Exception as e:
        return name, repr(e)


def main():
    census, pack, manifest, out = sys.argv[1:5]
    workers = int(sys.argv[5]) if len(sys.argv) > 5 else 3
    limit = int(sys.argv[6]) if len(sys.argv) > 6 else 0
    have = set()
    for m in json.load(open(manifest))['materials']:
        t, p, _, bits = m['semantic_key'].split(':')
        have.add((t, p))
    rows = [json.loads(l) for l in open(census) if l.strip()]
    todo, why = [], {}
    for r in rows:
        pic = r['picture']
        t, p = r['name'].split('-')[:2]
        reason = None
        if not pic: reason = 'no HD picture'
        elif r['material'] or (t, p) in have: reason = 'has a material'
        elif r['world'] == 0: reason = 'not drawn on the 3D world' if r['skinned'] == 0 else 'a character'
        elif r['skinned'] > r['world']: reason = 'a character'
        elif r['flat'] > r['world']: reason = 'mostly 2D'
        elif r['additive'] > 0.5 * (r['world'] + r['flat']): reason = 'additive (an effect)'
        elif r['w'] * r['h'] < 32 * 32: reason = 'tiny'
        why[r['name']] = reason or 'made'
        if not reason:
            todo.append((pic, os.path.join(pack, pic), out))
    os.makedirs(os.path.join(out, 'maps'), exist_ok=True)
    # the census's verdict on every texture, the list asked for
    with open(os.path.join(out, 'census-verdicts.csv'), 'w') as f:
        f.write('texture,picture,material,world,skinned,flat,additive,maps,first_room,verdict\n')
        for r in rows:
            f.write('%s,%s,%s,%d,%d,%d,%d,%s,%s,%s\n' % (r['name'], r['picture'], r['material'], r['world'], r['skinned'],
                    r['flat'], r['additive'], ' '.join(map(str, r['maps'])), r['room'], why[r['name']]))
    seen = set()
    todo = [j for j in todo if not (j[0] in seen or seen.add(j[0]))]
    if limit: todo = todo[:limit]
    print(len(rows), 'textures in the census,', len(todo), 'to make maps for', flush=True)
    done, failed = [], []
    t0 = time.time()
    with mp.Pool(workers) as pool:
        for i, (name, err) in enumerate(pool.imap_unordered(make, todo)):
            (failed if err else done).append((name, err))
            if (i + 1) % 25 == 0: print('%d/%d %.0fs' % (i + 1, len(todo), time.time() - t0), flush=True)
    mats = []
    for name, _ in sorted(done):
        stem = os.path.splitext(name)[0]
        t, p, bits = stem.split('-')[:3]
        mats.append({"semantic_key": "%s:%s:0x0:%s" % (t, p, bits), "role": "opaque_world_surface",
                     "material_family": "generated", "source": "deepbump",
                     "maps": {"normal": "maps/%s_normal.png" % stem, "orm": "maps/%s_orm.png" % stem},
                     "response": PARAMS})
    with open(os.path.join(out, 'manifest.json'), 'w') as f:
        json.dump({"schema": "pcsx2-material-replacements-v1", "game_serial": "SLUS-21168", "mode": "generated",
                   "enabled": True, "materials": mats}, f, indent=1)
    print('made', len(done), 'failed', len(failed), failed[:5], flush=True)


if __name__ == '__main__':
    mp.freeze_support()
    main()

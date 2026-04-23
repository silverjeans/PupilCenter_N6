"""Stitch all *_det.png under sim/out into one review grid."""
from pathlib import Path
import cv2
import numpy as np

HERE = Path(__file__).resolve().parent
OUT  = HERE / "out"

# Load all result PNGs in name order (handles Korean paths).
files = sorted(OUT.glob("*_det.png"))
if not files:
    raise SystemExit("no *_det.png in out/ — run sim.py first")

imgs = []
for p in files:
    buf = np.fromfile(str(p), dtype=np.uint8)
    img = cv2.imdecode(buf, cv2.IMREAD_COLOR)
    imgs.append(img)

# Normalize to the same size (smallest common, preserve aspect).
th, tw = min(i.shape[0] for i in imgs), min(i.shape[1] for i in imgs)
tiles = [cv2.resize(i, (tw, th)) for i in imgs]

cols = 5
rows = (len(tiles) + cols - 1) // cols
grid = np.full((rows * th, cols * tw, 3), 30, dtype=np.uint8)
for idx, tile in enumerate(tiles):
    r, c = divmod(idx, cols)
    grid[r*th:(r+1)*th, c*tw:(c+1)*tw] = tile

out_path = OUT / "montage.png"
ok, enc = cv2.imencode(".png", grid)
enc.tofile(str(out_path))
print(f"wrote {out_path}  ({rows}x{cols}, {grid.shape[1]}x{grid.shape[0]} px)")

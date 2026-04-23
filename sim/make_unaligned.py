"""
Generate a synthetic "unaligned" dataset by shifting each IR image so the
pupil lands in different regions of the frame.

Input  : C:/Users/USER/Downloads/눈IR/*.jpg
Output : sim/unaligned/<stem>_<tag>.png  + sim/unaligned/gt.csv

Pipeline per source image:
  1. Detect the pupil in the original (well-centred) image using the DLL
     with a generous centred ROI.  This gives us "ground truth" (GT) cx/cy.
  2. For each target offset (dx, dy) in a small set, translate the image
     so the pupil ends up at (orig_cx + dx, orig_cy + dy).  Clamp so the
     pupil stays fully inside the frame (with a safety margin so flood-fill
     doesn't touch the border and break).
  3. Save the shifted image as PNG and record the new GT coordinate.

We translate with cv2.warpAffine + BORDER_REPLICATE so the edges look
natural rather than leaving a sharp black band that would confuse the
detector with a new huge dark blob.
"""
from __future__ import annotations

import csv
import ctypes as C
from pathlib import Path

import cv2
import numpy as np

HERE   = Path(__file__).resolve().parent
SRC    = Path(r"C:/Users/USER/Downloads/눈IR")
OUT    = HERE / "unaligned"

# ---- DLL (reuse the sim's bindings locally to avoid importing sim.py as a
# module; keeps this script independent).
_dll = C.CDLL(str(HERE / "pupil_detect.dll"))
_dll.sim_configure.argtypes = [C.c_uint8, C.c_uint32, C.c_uint32, C.c_uint8, C.c_uint16]
_dll.sim_configure.restype  = C.c_int
_dll.sim_run_gray8.argtypes = [
    C.POINTER(C.c_uint8), C.c_uint16, C.c_uint16,
    C.c_int16, C.c_int16, C.c_uint16, C.c_uint16,
    C.POINTER(C.c_int16), C.POINTER(C.c_int16),
    C.POINTER(C.c_uint16), C.POINTER(C.c_uint32), C.POINTER(C.c_uint8),
]
_dll.sim_run_gray8.restype = C.c_int


def detect_gt(gray: np.ndarray):
    """Locate the pupil in a well-centred image. Morph-close first so
    corneal reflections don't punch holes. Returns (cx, cy, radius) or None."""
    k = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (11, 11))
    pre = cv2.morphologyEx(gray, cv2.MORPH_CLOSE, k)

    h, w = pre.shape
    # Centred 240x240 ROI works for this dataset (all source images are aligned)
    rx, ry, rw, rh = (w - 240) // 2, (h - 240) // 2, 240, 240
    buf = pre.ctypes.data_as(C.POINTER(C.c_uint8))
    cx, cy = C.c_int16(0), C.c_int16(0)
    r, a, cf = C.c_uint16(0), C.c_uint32(0), C.c_uint8(0)
    hit = _dll.sim_run_gray8(buf, w, h, rx, ry, rw, rh,
                             C.byref(cx), C.byref(cy),
                             C.byref(r), C.byref(a), C.byref(cf))
    if not hit:
        return None
    return int(cx.value), int(cy.value), int(r.value)


def shift_image(gray: np.ndarray, dx: int, dy: int) -> np.ndarray:
    h, w = gray.shape
    M = np.float32([[1, 0, dx], [0, 1, dy]])
    return cv2.warpAffine(gray, M, (w, h), borderMode=cv2.BORDER_REPLICATE)


def make_placements(w: int, h: int, pup_cx: int, pup_cy: int, radius: int):
    """Return a list of (tag, target_cx, target_cy) positions to move the
    pupil to.  Keep a safety margin of (radius + 8) from any border so
    flood-fill doesn't leak off-frame."""
    margin = max(radius, 30) + 8
    xs = {
        "L":  margin,
        "C":  w // 2,
        "R":  w - margin,
    }
    ys = {
        "T":  margin,
        "M":  h // 2,
        "B":  h - margin,
    }
    # 9 placements (T/M/B × L/C/R) but skip centre (MC) because that's
    # already well-covered by the aligned dataset.
    out = []
    for ytag, ty in ys.items():
        for xtag, tx in xs.items():
            tag = f"{ytag}{xtag}"
            if tag == "MC":
                continue
            out.append((tag, tx, ty))
    return out


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    # Params for GT detection (permissive; original images are easy).
    _dll.sim_configure(60, 500, 30000, 35, 10)

    rows = []
    srcs = sorted(SRC.glob("*.jpg"))
    skipped = 0
    for sp in srcs:
        raw = np.fromfile(str(sp), dtype=np.uint8)
        gray = cv2.imdecode(raw, cv2.IMREAD_GRAYSCALE)
        if gray is None:
            skipped += 1
            continue
        gt = detect_gt(gray)
        if gt is None:
            print(f"skip (no GT): {sp.name}")
            skipped += 1
            continue
        cx0, cy0, rad = gt
        h, w = gray.shape

        for tag, tx, ty in make_placements(w, h, cx0, cy0, rad):
            dx, dy = tx - cx0, ty - cy0
            shifted = shift_image(gray, dx, dy)
            out_name = f"{sp.stem}_{tag}.png"
            out_path = OUT / out_name
            ok, enc = cv2.imencode(".png", shifted)
            enc.tofile(str(out_path))

            rows.append({
                "file":    out_name,
                "src":     sp.name,
                "tag":     tag,
                "w":       w,
                "h":       h,
                "gt_cx":   tx,
                "gt_cy":   ty,
                "radius":  rad,
                "shift_dx": dx,
                "shift_dy": dy,
            })

    csv_path = OUT / "gt.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as fh:
        wr = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
        wr.writeheader()
        wr.writerows(rows)

    print(f"wrote {len(rows)} shifted images ({skipped} source skipped)")
    print(f"-> {OUT}")


if __name__ == "__main__":
    main()

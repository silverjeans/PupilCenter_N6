"""
Benchmark three SEARCH strategies on the shifted (unaligned) dataset.

  (A) baseline    : single centred 240x240 ROI (current firmware behaviour)
  (B) tiled       : scan the frame with 4 overlapping 256x256 tiles,
                    keep the first/best detection
  (C) downsample  : halve the frame (320x240), run detector once with a
                    256x240 ROI, then verify at full res with TRACK ROI

For each strategy we report:
  - detection rate (hit/total)
  - localisation error = Chebyshev distance from predicted centre to GT
  - pass/fail with a tolerance of 15 px (well inside one pupil radius)
  - average detector calls per frame (rough cost proxy)

Output: sim/out/bench_<strategy>.csv  and a summary printed to stdout.
"""
from __future__ import annotations

import csv
import ctypes as C
import time
from pathlib import Path

import cv2
import numpy as np

HERE = Path(__file__).resolve().parent
UNA  = HERE / "unaligned"
OUT  = HERE / "out"
TOL_PX = 15

# ---- DLL binding ----------------------------------------------------------
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


def call(gray: np.ndarray, rx, ry, rw, rh):
    h, w = gray.shape
    buf = gray.ctypes.data_as(C.POINTER(C.c_uint8))
    cx, cy = C.c_int16(0), C.c_int16(0)
    r, a, cf = C.c_uint16(0), C.c_uint32(0), C.c_uint8(0)
    hit = _dll.sim_run_gray8(buf, w, h, int(rx), int(ry), int(rw), int(rh),
                             C.byref(cx), C.byref(cy),
                             C.byref(r), C.byref(a), C.byref(cf))
    return (bool(hit),
            int(cx.value), int(cy.value),
            int(r.value), int(a.value), int(cf.value))


# ---- preprocessing (shared) ----------------------------------------------
_MORPH = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (11, 11))

def preprocess(gray: np.ndarray) -> np.ndarray:
    return cv2.morphologyEx(gray, cv2.MORPH_CLOSE, _MORPH)


# ---- strategies -----------------------------------------------------------

def strat_baseline(gray: np.ndarray):
    """Single 240x240 ROI centred on the frame."""
    h, w = gray.shape
    rx, ry, rw, rh = (w - 240) // 2, (h - 240) // 2, 240, 240
    hit, cx, cy, r, a, cf = call(gray, rx, ry, rw, rh)
    return hit, cx, cy, r, a, cf, 1


def strat_tiled(gray: np.ndarray):
    """Four 256x256 tiles with ~32 px overlap. Best hit by confidence wins."""
    h, w = gray.shape
    tw = 256
    # Place tiles so they cover the frame with overlap.
    xs = [0, max(0, w - tw)]
    ys = [0, max(0, h - tw)]
    best = None  # (conf, cx, cy, r, a)
    calls = 0
    for ty in ys:
        for tx in xs:
            rw = min(tw, w - tx)
            rh = min(tw, h - ty)
            if rw & 1: rw -= 1
            if rw < 8 or rh < 8:
                continue
            calls += 1
            hit, cx, cy, r, a, cf = call(gray, tx, ty, rw, rh)
            if hit and (best is None or cf > best[0]):
                best = (cf, cx, cy, r, a)
    if best is None:
        return False, 0, 0, 0, 0, 0, calls
    cf, cx, cy, r, a = best
    return True, cx, cy, r, a, cf, calls


def strat_downsample(gray: np.ndarray):
    """Halve the frame, detect in 256x240 ROI, then verify at full res
    with a tight 240x240 TRACK ROI around the scaled-up centre."""
    h, w = gray.shape
    small = cv2.resize(gray, (w // 2, h // 2), interpolation=cv2.INTER_AREA)
    sh, sw = small.shape
    # The detector's MAX_ROI is 256; use min(sw, 256), min(sh, 240).
    srw = min(sw, 256)
    if srw & 1: srw -= 1
    srh = min(sh, 240)
    srx = (sw - srw) // 2
    sry = (sh - srh) // 2

    calls = 1
    # At half-resolution the blob area scales by 1/4 and pupil fits easier.
    # We rely on the shared global config which is tuned for full-res, so
    # we temporarily relax area bounds for this call by re-configuring.
    _dll.sim_configure(60, 125, 8000, 35, 10)
    hit, scx, scy, sr, sa, scf = call(small, srx, sry, srw, srh)
    _dll.sim_configure(60, 500, 30000, 35, 10)

    if not hit:
        return False, 0, 0, 0, 0, 0, calls

    # Rescale to full-res coordinates and run a precise TRACK pass.
    fcx, fcy = scx * 2, scy * 2
    rw, rh = 240, 240
    rx = max(0, min(w - rw, fcx - rw // 2))
    ry = max(0, min(h - rh, fcy - rh // 2))
    if rx & 1: rx -= 1   # keep even for RGB565 alignment on target
    calls += 1
    hit2, cx, cy, r, a, cf = call(gray, rx, ry, rw, rh)
    if hit2:
        return True, cx, cy, r, a, cf, calls
    # Fall back to the coarse estimate if TRACK missed.
    return True, fcx, fcy, sr * 2, sa * 4, scf, calls


STRATEGIES = {
    "baseline":   strat_baseline,
    "tiled":      strat_tiled,
    "downsample": strat_downsample,
}


# ---- main -----------------------------------------------------------------

def load_gt():
    csv_path = UNA / "gt.csv"
    with csv_path.open("r", encoding="utf-8") as fh:
        return list(csv.DictReader(fh))


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    # Default config for full-res pathway.
    _dll.sim_configure(60, 500, 30000, 35, 10)

    gt_rows = load_gt()
    print(f"dataset: {len(gt_rows)} shifted images")
    print()

    summary = []
    for name, fn in STRATEGIES.items():
        hits = 0
        passes = 0
        total_calls = 0
        err_sum = 0.0
        err_hits = 0
        t0 = time.time()
        per_rows = []

        for row in gt_rows:
            path = UNA / row["file"]
            raw = np.fromfile(str(path), dtype=np.uint8)
            gray = cv2.imdecode(raw, cv2.IMREAD_GRAYSCALE)
            gray = preprocess(gray)

            hit, cx, cy, r, a, cf, calls = fn(gray)
            total_calls += calls
            gx = int(row["gt_cx"])
            gy = int(row["gt_cy"])
            err = max(abs(cx - gx), abs(cy - gy)) if hit else None
            if hit:
                hits += 1
                err_sum += err
                err_hits += 1
                if err <= TOL_PX:
                    passes += 1

            per_rows.append({
                "file": row["file"], "tag": row["tag"],
                "gt_cx": gx, "gt_cy": gy,
                "hit": int(hit), "cx": cx, "cy": cy,
                "err_px": -1 if err is None else err,
                "area": a, "conf": cf,
                "calls": calls,
            })

        dt = time.time() - t0
        avg_err = (err_sum / err_hits) if err_hits else float("nan")
        summary.append({
            "strategy":   name,
            "hit_rate":   f"{hits}/{len(gt_rows)} ({100*hits/len(gt_rows):.1f}%)",
            "pass<=15px": f"{passes}/{len(gt_rows)} ({100*passes/len(gt_rows):.1f}%)",
            "avg_err_px": f"{avg_err:.1f}" if err_hits else "n/a",
            "avg_calls":  f"{total_calls/len(gt_rows):.2f}",
            "time_s":     f"{dt:.2f}",
        })

        # Per-image csv
        with (OUT / f"bench_{name}.csv").open("w", newline="", encoding="utf-8") as fh:
            wr = csv.DictWriter(fh, fieldnames=list(per_rows[0].keys()))
            wr.writeheader()
            wr.writerows(per_rows)

    # Print summary
    col = ["strategy", "hit_rate", "pass<=15px", "avg_err_px", "avg_calls", "time_s"]
    widths = {c: max(len(c), max(len(s[c]) for s in summary)) for c in col}
    line = "  ".join(c.ljust(widths[c]) for c in col)
    print(line)
    print("-" * len(line))
    for s in summary:
        print("  ".join(s[c].ljust(widths[c]) for c in col))


if __name__ == "__main__":
    main()

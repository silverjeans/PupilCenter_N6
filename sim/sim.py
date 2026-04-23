"""
PupilCenter_N6 simulator - runs pupil_detect.dll against IR eye images
and writes annotated PNGs + a CSV log.

Usage:
    # single image
    python sim.py <image_path>

    # batch over a directory
    python sim.py <directory>

    # override detector params
    python sim.py <path> --thr 50 --min 500 --max 20000 --bias 35

    # full-frame ROI (default) or custom
    python sim.py <path> --roi 0,0,640,480
"""
from __future__ import annotations

import argparse
import csv
import ctypes as C
from pathlib import Path

import cv2
import numpy as np

HERE = Path(__file__).resolve().parent
DLL_PATH = HERE / "pupil_detect.dll"
OUT_DIR = HERE / "out"

# ---------- DLL binding -----------------------------------------------------

_dll = C.CDLL(str(DLL_PATH))

_dll.sim_configure.argtypes = [
    C.c_uint8, C.c_uint32, C.c_uint32, C.c_uint8, C.c_uint16
]
_dll.sim_configure.restype = C.c_int

_dll.sim_run_gray8.argtypes = [
    C.POINTER(C.c_uint8),          # data
    C.c_uint16, C.c_uint16,        # w, h
    C.c_int16, C.c_int16,          # roi x, y
    C.c_uint16, C.c_uint16,        # roi w, h
    C.POINTER(C.c_int16),          # out_cx
    C.POINTER(C.c_int16),          # out_cy
    C.POINTER(C.c_uint16),         # out_radius
    C.POINTER(C.c_uint32),         # out_area
    C.POINTER(C.c_uint8),          # out_conf
]
_dll.sim_run_gray8.restype = C.c_int


def configure(thr: int, amin: int, amax: int, bias: int, max_blobs: int = 10) -> None:
    rc = _dll.sim_configure(thr, amin, amax, bias, max_blobs)
    if rc != 0:
        raise RuntimeError(f"sim_configure failed rc={rc}")


def run_detect(gray: np.ndarray, roi):
    """Run detector on a uint8 grayscale frame; returns a result dict."""
    assert gray.dtype == np.uint8 and gray.ndim == 2
    h, w = gray.shape
    rx, ry, rw, rh = roi

    buf = gray.ctypes.data_as(C.POINTER(C.c_uint8))
    cx = C.c_int16(0)
    cy = C.c_int16(0)
    radius = C.c_uint16(0)
    area = C.c_uint32(0)
    conf = C.c_uint8(0)

    hit = _dll.sim_run_gray8(buf, w, h, rx, ry, rw, rh,
                             C.byref(cx), C.byref(cy),
                             C.byref(radius), C.byref(area), C.byref(conf))
    return {
        "hit": bool(hit),
        "cx": int(cx.value),
        "cy": int(cy.value),
        "radius": int(radius.value),
        "area": int(area.value),
        "conf": int(conf.value),          # 0..255
    }


# ---------- image pipeline --------------------------------------------------

def load_ir(path: Path) -> np.ndarray:
    # cv2.imread mangles non-ASCII paths on Windows; read raw bytes and
    # decode via imdecode to stay Korean-path safe.
    raw = np.fromfile(str(path), dtype=np.uint8)
    if raw.size == 0:
        raise FileNotFoundError(path)
    img = cv2.imdecode(raw, cv2.IMREAD_GRAYSCALE)
    if img is None:
        raise RuntimeError(f"decode failed: {path}")
    return img


def save_img(path: Path, bgr: np.ndarray) -> None:
    # imwrite has the same non-ASCII issue on Windows.
    ext = path.suffix.lower() or ".png"
    ok, buf = cv2.imencode(ext, bgr)
    if not ok:
        raise RuntimeError(f"encode failed: {path}")
    buf.tofile(str(path))


def annotate(gray: np.ndarray, roi, res) -> np.ndarray:
    """Return a BGR image with ROI rectangle and result overlay."""
    bgr = cv2.cvtColor(gray, cv2.COLOR_GRAY2BGR)
    rx, ry, rw, rh = roi
    cv2.rectangle(bgr, (rx, ry), (rx + rw, ry + rh), (0, 255, 255), 1)
    if res["hit"]:
        cx, cy, r = res["cx"], res["cy"], res["radius"]
        cv2.drawMarker(bgr, (cx, cy), (0, 0, 255),
                       markerType=cv2.MARKER_CROSS,
                       markerSize=40, thickness=2)
        if r > 0:
            cv2.circle(bgr, (cx, cy), r, (0, 255, 0), 1)
        label = f"cx={cx} cy={cy} area={res['area']} conf={res['conf']/255:.2f}"
    else:
        label = "no detection"
    cv2.putText(bgr, label, (8, 22), cv2.FONT_HERSHEY_SIMPLEX,
                0.6, (255, 255, 255), 1, cv2.LINE_AA)
    return bgr


def fill_glints(gray: np.ndarray, close_radius: int = 5) -> np.ndarray:
    """Morphological closing fills small bright spots (corneal reflections)
    that punch holes in the pupil and break 4-connected flood-fill."""
    if close_radius <= 0:
        return gray
    k = cv2.getStructuringElement(cv2.MORPH_ELLIPSE,
                                  (2 * close_radius + 1, 2 * close_radius + 1))
    return cv2.morphologyEx(gray, cv2.MORPH_CLOSE, k)


def process_one(path: Path, roi, csv_rows, close_r: int = 5) -> None:
    gray = load_ir(path)
    gray = fill_glints(gray, close_r)
    h, w = gray.shape
    rx, ry, rw, rh = roi if roi else (0, 0, w, h)
    # clamp user ROI to frame
    rx = max(0, min(rx, w - 1))
    ry = max(0, min(ry, h - 1))
    rw = max(2, min(rw, w - rx))
    rh = max(2, min(rh, h - ry))
    if rw % 2:
        rw -= 1

    res = run_detect(gray, (rx, ry, rw, rh))
    out = annotate(gray, (rx, ry, rw, rh), res)

    out_path = OUT_DIR / f"{path.stem}_det.png"
    save_img(out_path, out)

    csv_rows.append({
        "file": path.name,
        "w": w, "h": h,
        "roi_x": rx, "roi_y": ry, "roi_w": rw, "roi_h": rh,
        "hit": int(res["hit"]),
        "cx": res["cx"], "cy": res["cy"],
        "radius": res["radius"], "area": res["area"],
        "conf_255": res["conf"],
    })
    flag = "OK " if res["hit"] else "--- "
    print(f"{flag}{path.name:20s} cx={res['cx']:4d} cy={res['cy']:4d} "
          f"area={res['area']:6d} conf={res['conf']/255:.2f}")


# ---------- main ------------------------------------------------------------

def parse_roi(s):
    if s is None:
        return None
    xs = [int(v) for v in s.split(",")]
    if len(xs) != 4:
        raise argparse.ArgumentTypeError("roi must be x,y,w,h")
    return tuple(xs)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("target", help="image file or directory of .jpg")
    ap.add_argument("--thr", type=int, default=60,
                    help="dark threshold (0..252), default 60")
    ap.add_argument("--min", dest="amin", type=int, default=500,
                    help="min blob area px, default 500")
    ap.add_argument("--max", dest="amax", type=int, default=30000,
                    help="max blob area px, default 30000")
    ap.add_argument("--bias", type=int, default=35,
                    help="center bias 0..100, default 35")
    ap.add_argument("--roi", type=parse_roi, default=None,
                    help="force ROI x,y,w,h (default: full frame)")
    ap.add_argument("--close", type=int, default=5,
                    help="pre-closing radius to fill glints; 0 disables")
    args = ap.parse_args()

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    configure(args.thr, args.amin, args.amax, args.bias)
    print(f"config: thr={args.thr} area=[{args.amin},{args.amax}] bias={args.bias}")

    target = Path(args.target)
    if target.is_dir():
        files = sorted(target.glob("*.jpg")) + sorted(target.glob("*.png"))
    else:
        files = [target]

    if not files:
        print("no images found")
        return

    csv_rows = []
    for p in files:
        process_one(p, args.roi, csv_rows, args.close)

    csv_path = OUT_DIR / "results.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=list(csv_rows[0].keys()))
        w.writeheader()
        w.writerows(csv_rows)
    hits = sum(r["hit"] for r in csv_rows)
    print(f"\n{hits}/{len(csv_rows)} detected  ->  {OUT_DIR}")


if __name__ == "__main__":
    main()

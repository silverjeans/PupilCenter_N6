"""
Produce a single README-safe result image from the simulator output.

Why not ship the raw *_det.png ?
  The underlying IR eye photos are biometric data. Even the annotated
  overlay still shows iris texture that could identify the donor. This
  script takes one result PNG and heavily pixelates the eye region while
  keeping the ROI rectangle, crosshair, and detection circle visible, so
  the reader can see "the detector works" without seeing anyone's iris.
"""
from pathlib import Path
import cv2
import numpy as np

HERE = Path(__file__).resolve().parent
SRC  = HERE / "out" / "S5018R00_det.png"   # highest CONF in the batch
DST  = HERE.parent / "docs" / "sample_detection.png"


def mosaic(img: np.ndarray, block: int = 8) -> np.ndarray:
    h, w = img.shape[:2]
    small = cv2.resize(img, (max(1, w // block), max(1, h // block)),
                       interpolation=cv2.INTER_AREA)
    return cv2.resize(small, (w, h), interpolation=cv2.INTER_NEAREST)


def main():
    raw = np.fromfile(str(SRC), dtype=np.uint8)
    img = cv2.imdecode(raw, cv2.IMREAD_COLOR)
    if img is None:
        raise SystemExit(f"missing: {SRC}")

    # Mosaic the entire frame first so iris detail is destroyed.
    pix = mosaic(img, block=10)

    # Redraw crisp overlay on top so the algorithm output stays readable.
    # Re-read the annotation parameters from the CSV rather than re-running
    # the detector (the mosaic'd image would not feed it correctly).
    import csv
    with (HERE / "out" / "results.csv").open(encoding="utf-8") as fh:
        for row in csv.DictReader(fh):
            if row["file"] == SRC.name.replace("_det.png", ".jpg"):
                cx, cy = int(row["cx"]), int(row["cy"])
                r = int(row["radius"])
                rx, ry = int(row["roi_x"]), int(row["roi_y"])
                rw, rh = int(row["roi_w"]), int(row["roi_h"])
                conf = int(row["conf_255"]) / 255.0
                area = int(row["area"])
                break
        else:
            raise SystemExit("source not found in results.csv")

    cv2.rectangle(pix, (rx, ry), (rx + rw, ry + rh), (0, 255, 255), 2)
    cv2.circle(pix, (cx, cy), r, (0, 255, 0), 2)
    cv2.drawMarker(pix, (cx, cy), (0, 0, 255),
                   markerType=cv2.MARKER_CROSS,
                   markerSize=60, thickness=3)
    label = f"cx={cx} cy={cy}  area={area} px  conf={conf:.2f}"
    cv2.putText(pix, label, (12, 30), cv2.FONT_HERSHEY_SIMPLEX,
                0.8, (255, 255, 255), 2, cv2.LINE_AA)
    cv2.putText(pix, "(eye region pixelated for privacy)",
                (12, pix.shape[0] - 14), cv2.FONT_HERSHEY_SIMPLEX,
                0.55, (200, 200, 200), 1, cv2.LINE_AA)

    DST.parent.mkdir(parents=True, exist_ok=True)
    ok, enc = cv2.imencode(".png", pix)
    enc.tofile(str(DST))
    print(f"wrote {DST}")


if __name__ == "__main__":
    main()

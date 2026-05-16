#!/usr/bin/env python3
"""Read an uncompressed 32bpp BMP (as produced by screencapture -t bmp)
and emit luminance stats.

Usage: check_pixels.py <path.bmp> [--region x,y,w,h]
Output: single line of JSON.
Exit:   0 = rendered (varied content), 1 = black, 2 = unclear/error.

Thresholds tuned for "is the Steam window unrendered black":
  mean < 8 AND stddev < 8  -> black
  mean > 30 OR stddev > 30 -> rendered
  otherwise                -> unclear
"""
import json, struct, sys, argparse


def read_bmp(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:2] != b"BM":
        raise SystemExit(f"not a BMP: {path}")
    px_off = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    height_signed = struct.unpack_from("<i", data, 22)[0]
    bpp = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]
    if bpp != 32 or compression not in (0, 3):
        raise SystemExit(f"unsupported BMP: bpp={bpp} compression={compression}")
    height = abs(height_signed)
    top_down = height_signed < 0
    row_bytes = width * 4  # 4-byte aligned already at 32bpp
    return data, px_off, width, height, top_down, row_bytes


def stats(data, px_off, width, height, top_down, row_bytes, region):
    if region:
        rx, ry, rw, rh = region
        rx = max(0, min(rx, width))
        ry = max(0, min(ry, height))
        rw = max(0, min(rw, width - rx))
        rh = max(0, min(rh, height - ry))
    else:
        rx, ry, rw, rh = 0, 0, width, height

    total = rw * rh
    if total == 0:
        return None

    sum_lum = 0
    sum_sq = 0
    # Iterate rows; BMP rows are stored bottom-up unless top_down flag set.
    for j in range(rh):
        src_y = (ry + j) if top_down else (height - 1 - (ry + j))
        row_start = px_off + src_y * row_bytes + rx * 4
        row = data[row_start:row_start + rw * 4]
        # BGRA. Rec.601 luma approx: 0.114*B + 0.587*G + 0.299*R, fast int form.
        for i in range(rw):
            b = row[i*4]
            g = row[i*4+1]
            r = row[i*4+2]
            # Integer luma: (77*R + 150*G + 29*B) >> 8 ~ Rec.601
            y = (77*r + 150*g + 29*b) >> 8
            sum_lum += y
            sum_sq += y*y

    mean = sum_lum / total
    var = max(0, sum_sq / total - mean*mean)
    std = var ** 0.5
    return {"width": rw, "height": rh, "samples": total, "mean": mean, "stddev": std}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path")
    ap.add_argument("--region", help="x,y,w,h in pixel coords")
    args = ap.parse_args()

    region = None
    if args.region:
        try:
            region = tuple(int(v) for v in args.region.split(","))
            if len(region) != 4:
                raise ValueError
        except ValueError:
            raise SystemExit("--region must be x,y,w,h")

    data, px_off, width, height, top_down, row_bytes = read_bmp(args.path)
    s = stats(data, px_off, width, height, top_down, row_bytes, region)
    if s is None:
        print(json.dumps({"verdict": "error", "error": "empty region"}))
        sys.exit(2)

    mean, std = s["mean"], s["stddev"]
    if mean < 8 and std < 8:
        verdict = "black"
        rc = 1
    elif mean > 30 or std > 30:
        verdict = "rendered"
        rc = 0
    else:
        verdict = "unclear"
        rc = 2
    s["verdict"] = verdict
    print(json.dumps(s))
    sys.exit(rc)


if __name__ == "__main__":
    main()

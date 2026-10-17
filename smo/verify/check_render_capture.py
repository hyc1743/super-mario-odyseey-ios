#!/usr/bin/env python3
"""Fail the packaging gate when the captured game output is absent or black."""
import argparse
import json
from pathlib import Path


def read_ppm(path: Path):
    data = path.read_bytes()
    if not data.startswith(b"P6"):
        raise ValueError("capture is not binary PPM (P6)")
    pos = 2
    tokens = []
    while len(tokens) < 3:
        while pos < len(data) and data[pos] in b" \t\r\n":
            pos += 1
        if pos < len(data) and data[pos] == ord("#"):
            end = data.find(b"\n", pos)
            pos = len(data) if end < 0 else end + 1
            continue
        end = pos
        while end < len(data) and data[end] not in b" \t\r\n":
            end += 1
        if end == pos:
            raise ValueError("truncated PPM header")
        tokens.append(int(data[pos:end]))
        pos = end
    width, height, maximum = tokens
    if not 1 <= width <= 4096 or not 1 <= height <= 4096 or maximum != 255:
        raise ValueError(f"invalid PPM dimensions/maxval: {width}x{height}/{maximum}")
    while pos < len(data) and data[pos] in b" \t\r\n":
        pos += 1
    pixels = data[pos:]
    expected = width * height * 3
    if len(pixels) != expected:
        raise ValueError(f"PPM payload size {len(pixels)} != {expected}")
    return width, height, pixels


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("result", type=Path)
    args = parser.parse_args()
    result = json.loads(args.result.read_text(encoding="utf-8"))
    if not result.get("passed"):
        raise SystemExit("boot result did not pass")
    if result.get("test_press_a_count", 0) >= 2 and not result.get("gpu_frames_after_input"):
        raise SystemExit("no GPU frames after the automated A confirmation presses")
    width, height, pixels = read_ppm(args.capture)
    nonblack = 0
    minimum = 255
    maximum = 0
    for offset in range(0, len(pixels), 3):
        r, g, b = pixels[offset:offset + 3]
        value = max(r, g, b)
        minimum = min(minimum, value)
        maximum = max(maximum, value)
        if value >= 8:
            nonblack += 1
    count = width * height
    ratio = nonblack / count
    if ratio < 0.01:
        raise SystemExit(f"captured game output is nearly black: nonblack={ratio:.3%}")
    if maximum - minimum < 16:
        raise SystemExit(f"captured game output has insufficient range: {minimum}..{maximum}")
    print(json.dumps({"passed": True, "width": width, "height": height,
                      "nonblack_ratio": ratio, "range": [minimum, maximum]},
                     ensure_ascii=False))


if __name__ == "__main__":
    main()

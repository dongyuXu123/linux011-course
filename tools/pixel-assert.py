#!/usr/bin/env python3
"""对 QEMU screendump(PPM)做像素断言。
用法: pixel-assert.py screen.ppm "<x,y=hexcolor;...>"  (允许 ±8/通道误差)"""
import sys

def main():
    ppm, spec = sys.argv[1], sys.argv[2]
    data = open(ppm, "rb").read()
    assert data.startswith(b"P6")
    parts, i = [], 2
    while len(parts) < 3:
        while data[i:i+1].isspace(): i += 1
        if data[i:i+1] == b"#":
            while data[i:i+1] not in (b"\n", b""): i += 1
            continue
        j = i
        while j < len(data) and not data[j:j+1].isspace(): j += 1
        parts.append(int(data[i:j])); i = j
    w, h, _ = parts; i += 1
    raw = data[i:]
    fails = []
    for item in spec.split(";"):
        if not item.strip(): continue
        coord, color = item.split("=")
        x, y = (int(v) for v in coord.split(","))
        want = int(color, 16)
        wr, wg, wb = (want >> 16) & 0xFF, (want >> 8) & 0xFF, want & 0xFF
        o = (y * w + x) * 3
        gr, gg, gb = raw[o], raw[o+1], raw[o+2]
        if max(abs(gr-wr), abs(gg-wg), abs(gb-wb)) > 8:
            fails.append(f"({x},{y}) want {color} got {gr:02x}{gg:02x}{gb:02x}")
    if fails:
        print("PIXEL-FAIL: " + "; ".join(fails))
        sys.exit(1)
    print("PIXEL-OK")


if __name__ == "__main__":
    main()

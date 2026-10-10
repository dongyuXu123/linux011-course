#!/usr/bin/env python3
"""QEMU screendump 的 PPM(P6) 转 PNG，纯标准库。用法: ppm2png.py in.ppm out.png"""
import struct, sys, zlib

def main():
    src, dst = sys.argv[1], sys.argv[2]
    with open(src, "rb") as f:
        data = f.read()
    # 解析 P6 头: P6 <w> <h> <maxval>\n
    if not data.startswith(b"P6"):
        raise SystemExit("not P6")
    parts, i = [], 2
    while len(parts) < 3:
        while i < len(data) and data[i:i+1].isspace():
            i += 1
        if data[i:i+1] == b"#":
            while data[i:i+1] not in (b"\n", b""):
                i += 1
            continue
        j = i
        while j < len(data) and not data[j:j+1].isspace():
            j += 1
        parts.append(int(data[i:j])); i = j
    w, h, _maxval = parts
    i += 1  # 单个空白
    raw = data[i:i + w * h * 3]

    def chunk(tag, payload):
        c = struct.pack(">I", len(payload)) + tag + payload
        return c + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    scan = b"".join(b"\x00" + raw[y*w*3:(y+1)*w*3] for y in range(h))
    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
           + chunk(b"IDAT", zlib.compress(scan, 6)) + chunk(b"IEND", b""))
    with open(dst, "wb") as f:
        f.write(png)

if __name__ == "__main__":
    main()

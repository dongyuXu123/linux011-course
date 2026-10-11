#!/usr/bin/env python3
"""mkinitrd.py <output> <name=文件> ... —— 打包课程 initrd (romfs-lite)
格式: 循环 { 4B name_len | name | 4B data_len | data } 直到 EOF"""
import struct, sys

out = sys.argv[1]
buf = b""
for spec in sys.argv[2:]:
    name, _, path = spec.partition("=")
    data = open(path, "rb").read()
    nb = name.encode()
    buf += struct.pack("<I", len(nb)) + nb + struct.pack("<I", len(data)) + data
open(out, "wb").write(buf)
print(f"initrd: {out} ({len(buf)} bytes)")

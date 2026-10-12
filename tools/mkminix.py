#!/usr/bin/env python3
# =====================================================================
# mkminix.py —— 在宿主上制作 Minix v1 文件系统镜像(day34)
# 结构逐字段对照 0.11 include/linux/minix_fs_sb.h / fs.h:
#   引导块(1KB) | 超级块(1KB) | imap | zmap | inode 表 | 数据区
#   超级块魔数 0x137F, 32B 的 minix inode, 数据区 1KB/zone
# 课程范围: 单层根目录, 文件 ≤7 个直接块(一级间接结构留位, 工具不生成)
# =====================================================================
import struct, sys

BS        = 1024
MAGIC     = 0x137F
NI        = 64          # inode 总数
NZONES    = 512         # 数据 zone 总数
INODE_TBL_BLOCKS = NI * 32 // BS
IMAP_BLOCKS = 1
ZMAP_BLOCKS = 1

SB_BLOCK = 1
IMAP0    = 2
ZMAP0    = IMAP0 + IMAP_BLOCKS
ITBL0    = ZMAP0 + ZMAP_BLOCKS
DATA0    = ITBL0 + INODE_TBL_BLOCKS   # 首个数据 zone 的块号

def build(files):
    img = bytearray((DATA0 + NZONES) * BS)

    # 严格按 0.11 struct minix_super_block:
    # u16 n_inodes; u16 n_zones; u16 imap_blocks; u16 zmap_blocks;
    # u16 firstdatazone; u16 log_zone_size; u32 max_size; u32 magic;
    # u32 state; u32 zones;
    sb = struct.pack('<HHHHHHIIII',
        NI, NZONES, IMAP_BLOCKS, ZMAP_BLOCKS,
        DATA0, 0,
        (7 + 512) * BS,
        MAGIC, 1, NZONES)
    img[SB_BLOCK*BS:SB_BLOCK*BS+len(sb)] = sb

    inodes = [None] * (NI + 1)       # 1-based
    imap   = bytearray(IMAP_BLOCKS * BS)
    zmap   = bytearray(ZMAP_BLOCKS * BS)

    def set_bit(arr, i):
        arr[i >> 3] |= 1 << (i & 7)

    set_bit(imap, 0)                 # 0 号保留(0.11 位图 bit0 恒 1)
    for z in range(DATA0):
        set_bit(zmap, z)

    def alloc_zone():
        for z in range(DATA0, NZONES):
            if not (zmap[z >> 3] >> (z & 7)) & 1:
                set_bit(zmap, z)
                return z
        raise RuntimeError("out of zones")

    def wr_zone(z, data):
        assert len(data) <= BS
        img[z*BS:z*BS+len(data)] = data

    def add_file(name, data):
        assert len(data) <= 7 * BS, "course mkminix: direct blocks only"
        ino = next(i for i in range(2, NI + 1) if inodes[i] is None)  # 1=根目录
        set_bit(imap, ino)
        nblk = (len(data) + BS - 1) // BS
        zones = [alloc_zone() for _ in range(nblk)]
        for k in range(nblk):
            wr_zone(zones[k], data[k*BS:(k+1)*BS])
        inodes[ino] = {
            'mode': 0x80FF,            # 常规文件 S_IFREG|0777
            'uid': 0, 'size': len(data), 'mtime': 0, 'gid': 0, 'nlinks': 1,
            'zones': (zones + [0] * (9 - len(zones))),
        }
        return ino

    root_entries = []
    for name, data in files:
        root_entries.append((add_file(name, data), name))

    # ---- 根目录 = 1 号 inode(0.11 由 mknod 建 "/", 这里硬编码) ----
    set_bit(imap, 1)
    dentries = b''
    for ino, name in root_entries:
        dentries += struct.pack('<H21sx', ino, name.encode())  # 24B/项
    dentries += struct.pack('<H21sx', 1, b'.')
    dentries += struct.pack('<H21sx', 1, b'..')
    dentries += b'\x00' * (-len(dentries) % BS)
    root_zone = alloc_zone()
    wr_zone(root_zone, dentries)
    inodes[1] = {
        'mode': 0x41FF,                # 目录 S_IFDIR|0777
        'uid': 0, 'size': len(dentries), 'mtime': 0, 'gid': 0, 'nlinks': 3,
        'zones': [root_zone, 0, 0, 0, 0, 0, 0, 0, 0],
    }

    # ---- inode 表: v1 = 32B [u16 mode; u16 uid; u32 size; u32 mtime;
    #      u8 gid; u8 nlinks; u16 zone[9]] ----
    itbl = bytearray(INODE_TBL_BLOCKS * BS)
    for i in range(1, NI + 1):
        d = inodes[i]
        if not d:
            continue
        struct.pack_into('<HHIIBBH', itbl, (i - 1) * 32,
                         d['mode'], d['uid'], d['size'], d['mtime'],
                         d['gid'], d['nlinks'], 0)
        struct.pack_into('<9H', itbl, (i - 1) * 32 + 14, *d['zones'])
    img[ITBL0*BS:(ITBL0+INODE_TBL_BLOCKS)*BS] = itbl
    img[IMAP0*BS:IMAP0*BS+IMAP_BLOCKS*BS] = imap
    img[ZMAP0*BS:ZMAP0*BS+ZMAP_BLOCKS*BS] = zmap
    return bytes(img)

if __name__ == '__main__':
    out = sys.argv[1]
    files = []
    for spec in sys.argv[2:]:
        name, path = spec.split('=', 1)
        files.append((name, open(path, 'rb').read()))
    files.append(('welcome.txt', b'welcome to day34 minix v1 filesystem\n'))
    files.append(('motd', b'day34: superblock+bitmap+inode+buffer cache\n'))
    img = build(files)
    open(out, 'wb').write(img)
    print(f"mkminix: {out} {len(img)} bytes, {len(files)} files")

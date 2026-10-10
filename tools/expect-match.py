#!/usr/bin/env python3
"""对串口日志做顺序断言：expect.txt 中每行(去注释/空行)必须按顺序出现在日志中。
轮询日志文件直到全部命中或超时。
用法: expect-match.py <serial.log> <expect.txt> <timeout-秒>
退出码: 0=全部命中  2=超时(部分命中)  3=文件错误
stdout: 已命中数/总数 + 耗时
"""
import sys, time

def patterns(path):
    out = []
    for raw in open(path, encoding="utf-8"):
        line = raw.rstrip("\n")
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        out.append(line)
    return out

def main():
    log_path, exp_path, timeout = sys.argv[1], sys.argv[2], float(sys.argv[3])
    pats = patterns(exp_path)
    if not pats:
        print("0/0")
        return
    deadline = time.monotonic() + timeout
    idx = 0          # 下一个待命中的模式
    pos = 0          # 日志中的消费位置(字节)
    buf = b""
    start = time.monotonic()
    while idx < len(pats) and time.monotonic() < deadline:
        try:
            with open(log_path, "rb") as f:
                f.seek(pos)
                chunk = f.read()
        except FileNotFoundError:
            chunk = b""
        if chunk:
            buf = (buf + chunk)[-65536:]
            pos += len(chunk)
        while idx < len(pats):
            pat = pats[idx].encode("utf-8", "replace")
            hit = buf.find(pat)
            if hit < 0:
                break
            buf = buf[hit + len(pat):]
            idx += 1
        if idx < len(pats):
            time.sleep(0.05)
    dt = time.monotonic() - start
    print(f"{idx}/{len(pats)} {dt:.1f}s")
    sys.exit(0 if idx == len(pats) else 2)

if __name__ == "__main__":
    main()

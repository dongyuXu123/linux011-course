#!/usr/bin/env python3
"""极简 QMP 客户端：连接 QEMU 的 QMP unix socket 执行命令。
用法: qmp.py <socket> <command> [args-json]
示例: qmp.py /tmp/q.sock screendump '{"filename":"/tmp/s.ppm"}'
"""
import json, socket, sys

def main():
    sock_path, command = sys.argv[1], sys.argv[2]
    args = json.loads(sys.argv[3]) if len(sys.argv) > 3 else {}
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(15)
    s.connect(sock_path)
    f = s.makefile("rw", encoding="utf-8", newline="\n")

    def recv():
        while True:
            line = f.readline()
            if not line:
                raise RuntimeError("qmp closed")
            msg = json.loads(line)
            if "event" in msg:
                continue
            return msg

    recv()  # greeting
    f.write(json.dumps({"execute": "qmp_capabilities"}) + "\n"); f.flush()
    recv()
    f.write(json.dumps({"execute": command, "arguments": args}) + "\n"); f.flush()
    resp = recv()
    if "error" in resp:
        print(json.dumps(resp["error"]), file=sys.stderr)
        sys.exit(2)
    print(json.dumps(resp.get("return", {})))
    try:
        f.write(json.dumps({"execute": "quit"}) + "\n"); f.flush()
    except Exception:
        pass

if __name__ == "__main__":
    main()

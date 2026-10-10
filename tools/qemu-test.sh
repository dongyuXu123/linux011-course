#!/usr/bin/env bash
# ============ 每课 QEMU 自动验收 ============
# 用法: qemu-test.sh --image <os.iso|os.img> [--expect test/expect.txt]
#                    [--timeout 30] [--name dayNN] [--screen 1|0] [--extra-qemu-args "..."]
# 产物: <image目录>/test/{serial.log, screen.png, result.json}
# 退出码: 0=PASS 1=FAIL
set -u

IMAGE=""; EXPECT=""; TIMEOUT=45; NAME="test"; SCREEN=1; EXTRA=""
while [ $# -gt 0 ]; do
  case "$1" in
    --image) IMAGE="$2"; shift 2;;
    --expect) EXPECT="$2"; shift 2;;
    --timeout) TIMEOUT="$2"; shift 2;;
    --name) NAME="$2"; shift 2;;
    --screen) SCREEN="$2"; shift 2;;
    --extra-qemu-args) EXTRA="$2"; shift 2;;
    *) echo "unknown arg $1" >&2; exit 3;;
  esac
done
[ -n "$IMAGE" ] && [ -f "$IMAGE" ] || { echo "no image: $IMAGE" >&2; exit 3; }

REPO="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$(dirname "$IMAGE")/test"
mkdir -p "$OUT"
SERIAL="$OUT/serial.log"
SOCK="$OUT/qmp.sock"
rm -f "$SERIAL" "$SOCK" "$OUT/screen.ppm" "$OUT/screen.png"

DRIVE=(); MEDIA_ARGS=()
case "$IMAGE" in
  *.iso) MEDIA_ARGS=(-cdrom "$IMAGE" -boot d);;
  *.img) MEDIA_ARGS=(-fda "$IMAGE" -boot a);;
  *) echo "unsupported image type" >&2; exit 3;;
esac

timeout -k 5 $((TIMEOUT + 10)) \
  qemu-system-i386 -m 32M -display none -vga std -no-reboot \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
    -qmp unix:"$SOCK",server,nowait \
    -serial stdio "${MEDIA_ARGS[@]}" $EXTRA \
    >"$SERIAL" 2>"$OUT/qemu.err" &
QPID=$!
for i in $(seq 1 50); do [ -S "$SOCK" ] && break; sleep 0.1; done

STATUS=FAIL; MATCHED="?"; DUR=0
if [ -n "$EXPECT" ] && [ -f "$EXPECT" ]; then
  MATCH_OUT=$(python3 "$REPO/tools/expect-match.py" "$SERIAL" "$EXPECT" "$TIMEOUT")
  MATCHED="${MATCH_OUT%% *}"
  RC=$?
  DUR=$(echo "$MATCH_OUT" | awk '{print $2}')
  if [ "$RC" -eq 0 ]; then STATUS=PASS; fi
else
  STATUS=PASS; MATCHED="n/a"
fi

if [ "$SCREEN" = "1" ] && [ -S "$SOCK" ]; then
  python3 "$REPO/tools/qmp.py" "$SOCK" screendump "{\"filename\":\"$OUT/screen.ppm\"}" >/dev/null 2>&1 \
    && python3 "$REPO/tools/ppm2png.py" "$OUT/screen.ppm" "$OUT/screen.png" 2>/dev/null
fi
if [ -S "$SOCK" ]; then
  python3 "$REPO/tools/qmp.py" "$SOCK" quit >/dev/null 2>&1
fi
wait "$QPID" 2>/dev/null; QRC=$?

printf '{"name":"%s","status":"%s","matched":"%s","timeout":%s,"seconds":"%s","qemu_exit":%d}\n' \
  "$NAME" "$STATUS" "$MATCHED" "$TIMEOUT" "${DUR:-0}" "$QRC" > "$OUT/result.json"

if [ "$STATUS" = "PASS" ]; then
  echo "PASS [$NAME] matched=$MATCHED"
  exit 0
else
  echo "FAIL [$NAME] matched=$MATCHED (详见 $OUT)"
  exit 1
fi

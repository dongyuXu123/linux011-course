#!/usr/bin/env bash
# ============ 每课 QEMU 自动验收 ============
# 用法: qemu-test.sh --image <os.iso|os.img> [--expect test/expect.txt]
#                    [--timeout 30] [--name dayNN] [--screen 1|0] [--extra-qemu-args "..."]
# 产物: <image目录>/test/{serial.log, screen.png, result.json}
# 退出码: 0=PASS 1=FAIL
set -u

IMAGE=""; EXPECT=""; TIMEOUT=45; NAME="test"; SCREEN=1; EXTRA=""; KEYS=""; PIXEL_SPEC=""; MOUSE_SPEC=""
while [ $# -gt 0 ]; do
  case "$1" in
    --image) IMAGE="$2"; shift 2;;
    --expect) EXPECT="$2"; shift 2;;
    --timeout) TIMEOUT="$2"; shift 2;;
    --name) NAME="$2"; shift 2;;
    --screen) SCREEN="$2"; shift 2;;
    --extra-qemu-args) EXTRA="$2"; shift 2;;
    --send-keys) KEYS="$2"; shift 2;;
    --pixel) PIXEL_SPEC="$2"; shift 2;;
    --mouse) MOUSE_SPEC="$2"; shift 2;;
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

DRIVE=()
case "$IMAGE" in
  *.iso) DRIVE=(-drive "file=$IMAGE,media=cdrom,format=raw" -boot d);;
  *.img) DRIVE=(-drive "file=$IMAGE,if=floppy,format=raw" -boot a);;
  *) echo "unsupported image type" >&2; exit 3;;
esac

timeout -k 5 $((TIMEOUT + 10)) \
  qemu-system-i386 -m 32M -display none -vga std -no-reboot \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
    -qmp unix:"$SOCK",server,nowait \
    -serial stdio "${DRIVE[@]}" $EXTRA \
    >"$SERIAL" 2>"$OUT/qemu.err" &
QPID=$!
for i in $(seq 1 50); do [ -S "$SOCK" ] && break; sleep 0.1; done

# 鼠标注入: --mouse "dx,dy[;dx,dy;dx,dy,down-left;dx,dy,up-left]"
# 多步序列, 步间 1.5s; 事件 = 相对位移 + 可选按键按下/抬起
if [ -n "$MOUSE_SPEC" ] && [ -S "$SOCK" ]; then
  sleep 4
  IFS=';' read -ra MSTEPS <<< "$MOUSE_SPEC"
  for st in "${MSTEPS[@]}"; do
    [ -z "$st" ] && continue
    DX="${st%%,*}"; REST="${st#*,}"
    DY="${REST%%,*}"; ACT="${REST#*,}"
    [ "$ACT" = "$REST" ] && ACT=""
    EV="{\"type\":\"rel\",\"data\":{\"axis\":\"x\",\"value\":$DX}},{\"type\":\"rel\",\"data\":{\"axis\":\"y\",\"value\":$DY}}"
    case "$ACT" in
      down-left)  EV="$EV,{\"type\":\"btn\",\"data\":{\"down\":true,\"button\":\"left\"}}";;
      up-left)    EV="$EV,{\"type\":\"btn\",\"data\":{\"down\":false,\"button\":\"left\"}}";;
      click-left) EV="$EV,{\"type\":\"btn\",\"data\":{\"down\":true,\"button\":\"left\"}},{\"type\":\"btn\",\"data\":{\"down\":false,\"button\":\"left\"}}";;
    esac
    python3 "$REPO/tools/qmp.py" "$SOCK" input-send-event "{\"events\":[$EV]}" >/dev/null
    sleep 1.5
  done
  sleep 1
fi

# 按 qcode 逗号列表向 guest 发键(键盘课用): --send-keys "h,i,ret"
if [ -n "$KEYS" ] && [ -S "$SOCK" ]; then
  sleep 3                       # 等 GRUB 引导完, 太早的键会被丢掉
  IFS=',' read -ra KEYLIST <<< "$KEYS"
  for k in "${KEYLIST[@]}"; do
    if ! python3 "$REPO/tools/qmp.py" "$SOCK" send-key "{\"keys\":[{\"type\":\"qcode\",\"data\":\"$k\"}]}" >/dev/null; then
      echo "WARN: send-key '$k' 失败(检查 qcode 拼写)" >&2
    fi
    sleep 0.8
  done
fi

STATUS=FAIL; MATCHED="?"; DUR=0
if [ -n "$EXPECT" ] && [ -f "$EXPECT" ]; then
  RC=0
  MATCH_OUT=$(python3 "$REPO/tools/expect-match.py" "$SERIAL" "$EXPECT" "$TIMEOUT") || RC=$?
  MATCHED="${MATCH_OUT%% *}"
  DUR=$(echo "$MATCH_OUT" | awk '{print $2}')
  if [ "$RC" -eq 0 ]; then STATUS=PASS; fi
else
  STATUS=PASS; MATCHED="n/a"
fi

if [ "$SCREEN" = "1" ] && [ -S "$SOCK" ]; then
  python3 "$REPO/tools/qmp.py" "$SOCK" screendump "{\"filename\":\"$OUT/screen.ppm\"}" >/dev/null 2>&1 \
    && python3 "$REPO/tools/ppm2png.py" "$OUT/screen.ppm" "$OUT/screen.png" 2>/dev/null
  if [ -n "$PIXEL_SPEC" ] && [ -f "$OUT/screen.ppm" ]; then
    if python3 "$REPO/tools/pixel-assert.py" "$OUT/screen.ppm" "$PIXEL_SPEC"; then
      :
    else
      STATUS=FAIL
    fi
  fi
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

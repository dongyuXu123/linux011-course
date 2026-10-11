#!/bin/bash
# 启动 Xvfb(虚拟显示) + Xfce 会话 + x11vnc + noVNC(浏览器访问)
set -e
export DISPLAY=:1

Xvfb :1 -screen 0 1280x800x24 &
sleep 1
dbus-launch --exit-with-session startxfce4 &
sleep 3
x11vnc -display :1 -forever -nopass -shared -rfbport 5901 &
websockify --web /usr/share/novnc 8080 localhost:5901 &
echo "================================================"
echo " noVNC:   http://localhost:8080/vnc.html"
echo " VNC:     localhost:5901"
echo " 课程:    cd /course && make -C days/day01-hello-mbr test"
echo " 图形运行: make -C days/day30-release run"
echo "================================================"
wait -n

# Xfce + noVNC 可复现实验环境
#
# 浏览器打开 http://localhost:8080 即得完整 Xfce 桌面:
# 终端里 cd /course && make -C days/day21-framebuffer-vbe run
# 可看到 QEMU 图形窗口(课程桌面), 键盘鼠标直接操作。
#
# 构建并启动:
#   docker build -t linux011-course env/
#   docker run -p 8080:8080 -p 5901:5901 linux011-course
# (容器内工具链版本固定, 与 mk/ 构建体系一致)
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN sed -i 's|http://archive.ubuntu.com|http://mirrors.tuna.tsinghua.edu.cn|g; s|http://security.ubuntu.com|http://mirrors.tuna.tsinghua.edu.cn|g' /etc/apt/sources.list.d/ubuntu.sources 2>/dev/null || true

RUN apt-get update && apt-get install -y --no-install-recommends \
    xfce4 xfce4-terminal xfce4-goodies dbus-x11 x11vnc xvfb xterm \
    novnc websockify \
    build-essential gcc-multilib nasm mtools grub-pc-bin xorriso cpio \
    qemu-system-x86 python3 git sudo \
    && rm -rf /var/lib/apt/lists/*

# 课程代码挂载点(亦可在构建时 COPY . /course)
WORKDIR /course

# VNC 密码为空(仅本地实验); x11vnc 绑定虚拟显示 :1
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

EXPOSE 8080 5901
ENTRYPOINT ["/entrypoint.sh"]

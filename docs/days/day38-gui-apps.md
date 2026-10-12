# day38 - 用户态图形应用:时钟与文件浏览器

day37 打通了"应用经系统调用建窗"。今天把角色彻底翻转: **内核不再
拥有应用** —— day30 的内核侧 apps.c(时钟/内存监视)退役, 时钟与
文件浏览器改写为两个 ring3 程序(clock/files), 经 fork+execve 启动,
经 win_create=41 / win_write=42 / time=13 / open-read-close 工作。
内核回归它该有的样子: 只做合成器。

## 已实现并验证(串口 25/25 + 像素断言 OK)

- `user/clock.c`: 建窗(330,160 200x90) → time=13(0.11 同号,
  jiffies/100) 每秒一次把 "uptime Ns" 写进窗口 → 5 tick 后 exit
  —— 30dayMakeOS day21(定时器/窗口文字)的进程版
- `user/files.c`: 文件浏览器(列表版) —— open=5/read=3/close=6
  逐个读 motd/notes.txt/welcome.txt 首行, win_write 进自己的窗口;
  打不开的显示 -x(day35 文件 ABI 与 day37 窗口 API 的合体)
- `init` 新增 clock/files 命令( fork + execve + wait4 )
- 内核侧 apps.c 退役: gui_refresh 不再调 apps_render; MAXWINS=4
  的窗口额度全部让给用户应用
- 像素断言随布局更新(590,258 现在落在 files 窗)

串口证据链: `files: starting` → `win 2 created … 'files'` → 三行
win_write(marker px=ff00ff×3) → `app child 2 exited with 0` →
`clock: starting` → `win 3 created … 'clock'` → 5 tick → exit。

## 调试实录

- 期望里写死 "app child **2** exited" —— 第二个应用的 fork 拿到
  **pid 3**(pid 计数器不复用, 0.11 同语义)。教训: 断言进程相关
  输出前先想清楚 pid/uid 这些"计数器语义"。
- 窗口 id 从 2 起算: 终端窗 uid=1, 内核应用退役后 uid 计数不变。

## 对照阅读

- 30dayMakeOS day21-25: 窗口内文字/定时器/终端 —— 我们的对应物
  全部运行在 ring3
- `source/fs/`无窗口 —— 图形是课程扩展; 应用形态对应 0.11 的
  用户态守护进程

## 课程简化

- clock 用忙等轮询 time(无 sleep/hlt 睡眠接口); files 只列固定
  三个文件(无 readdir —— day35 未做目录读系统调用)

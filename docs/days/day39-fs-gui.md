# day39 - 文件操作进图形 + 窗口焦点轮转

终端窗正式成为 **fs 背后的图形 shell**: init 的 `w <file> <text>`
(creat+write)与 `cat <file>`(open+read)把 0.11 文件 ABI 接到键盘和
窗口; `next` 命令触发 `win_focus_next=43`(Alt-Tab 语义的焦点轮转,
经系统调用暴露 —— QMP 无法注入组合键, 课程以命令代击键)。

## 已实现并验证(串口 21/21 + 像素断言 OK)

- `init`: `w`/`cat` 命令(day15 shell 语义的用户态重生), 全部走
  day35 的 0.11 文件 ABI; `next` 调 43 号调用
- `gui_focus_next`: 焦点窗口挪到 z 序末尾、原后继获得焦点并置顶;
  `sys_win_focus` 触发后串口出 `[day39] focus -> <title>`
- 镜像 freshness: minix.img 加 FORCE 依赖 —— 写测试依赖"每次启动
  盘是新的"(持久化写入会污染下一次运行)
- 像素断言更新为轮转后布局(失焦标题变暗色/任务栏背景)

## 调试实录: 一个自引用结构体的教科书陷阱

焦点轮转永远"切回原窗口"。三层排开:
1. 轮转算法写反(把 cur 换到末尾后又聚焦它) —— 修;
2. printk 还在读 `wins[nwins-1].title`(被换走的窗口) —— 修;
3. 修完仍打印错名: **`wins[i] = wins[j]` 结构体拷贝把 `title` 指针
   原样带走, 它指向旧槽位的 tbuf** —— 拷贝后所有 title 都该修回
   `wins[i].title = wins[i].tbuf`。day30 的拖动换位同带此雷, 当年
   标题是字符串字面量(恒定地址)所以没炸; day37 起标题来自调用方
   缓冲、必须自持, 雷就爆了。新增 `wins_repair_titles()` 在任意
   wins[] 挪动后调用。

同场加映: 窗口标题显示成了别的窗口名 —— 系统调用侧把 `kbuf`(共享
静态缓冲)当 title 传进来, 后续任何系统调用都会改写它。修复: struct
win 自带 `tbuf[16]`, win_create 拷贝标题。教训: *跨"调用"保存的
指针必须指向自持存储, 共享暂存区是定时炸弹*。

## 对照阅读

- 30dayMakeOS day27-28(窗口切换加速/多终端输入路由)的进程版
- `source/fs/`文件语义已由 day35 承担; 本章把"文件"接进"图形"

## 课程简化

- 焦点轮转以命令代替 Alt-Tab(QMP 组合键限制); 无 shift 键映射
- cat 只读首 127 字节; w 覆盖写(无追加)

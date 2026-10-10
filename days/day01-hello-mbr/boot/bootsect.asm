; =====================================================================
; day01 - hello-mbr：512 字节引导扇区
; ---------------------------------------------------------------------
; 计算机加电后，BIOS 把磁盘第一个扇区(512字节)搬到 0x7C00 并跳转过去，
; 前提是扇区末尾有魔数 0xAA55。本课的"内核"就是这 512 字节：
;   1. 初始化串口 COM1 (115200 8N1)          —— 课程所有测试的输出通道
;   2. 向串口打印问候语                       —— QEMU 自动验收依据
;   3. 用 BIOS INT 10H 在屏幕上打印同样内容   —— make run 时人眼可见
;
; 对照阅读: source/boot/bootsect.s —— 0.11 的引导扇区做的是同一件事的
; "加强版": 把自己搬到 0x90000、再从磁盘把 setup 和 system 读进来。
; =====================================================================
[BITS 16]
[ORG 0x7C00]

COM1        equ 0x3F8

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00           ; 栈往下长，正好顶着自己
    sti

    call serial_init

    mov si, msg_serial
    call puts_serial

    mov si, msg_screen
    call puts_bios

    mov si, msg_bye
    call puts_serial

.halt:
    hlt
    jmp .halt

; ---- 初始化 COM1: 115200 波特, 8 数据位, 无校验, 1 停止位 ----
serial_init:
    mov dx, COM1 + 1
    xor al, al               ; 关中断
    out dx, al
    mov dx, COM1 + 3
    mov al, 0x80             ; DLAB=1, 允许设置波特率
    out dx, al
    mov dx, COM1
    mov al, 0x01             ; 除数低字节 = 1 → 115200
    out dx, al
    mov dx, COM1 + 1
    xor al, al               ; 除数高字节 = 0
    out dx, al
    mov dx, COM1 + 3
    mov al, 0x03             ; DLAB=0, 8N1
    out dx, al
    mov dx, COM1 + 2
    mov al, 0xC7             ; FIFO 开启并清空
    out dx, al
    mov dx, COM1 + 4
    mov al, 0x0B             ; DTR+RTS+OUT2
    out dx, al
    ret

; ---- 向串口打印以 0 结尾的字符串 (SI=指针) ----
puts_serial:
    lodsb
    test al, al
    jz .done
    call putc_serial
    jmp puts_serial
.done:
    ret

putc_serial:
    push ax
    mov dx, COM1 + 5
.wait:
    in al, dx                ; 等待发送保持寄存器空
    test al, 0x20
    jz .wait
    pop ax
    mov dx, COM1
    out dx, al
    ret

; ---- BIOS INT 10H 打字 ( make run 时在 QEMU 窗口可见 ) ----
puts_bios:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    jmp puts_bios
.done:
    ret

msg_serial db 13, 10, "[day01] hello from MBR (COM1 115200)", 13, 10, 0
msg_screen db "day01: MBR is running on screen too", 0
msg_bye    db "[day01] boot sector done, halt.", 13, 10, 0

times 510 - ($ - $$) db 0   ; 填充到 510 字节
dw 0xAA55                    ; 引导魔数

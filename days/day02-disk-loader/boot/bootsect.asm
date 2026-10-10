; =====================================================================
; day02 - disk-loader：引导扇区从磁盘把"内核"读进内存
; ---------------------------------------------------------------------
; BIOS 只负责加载第一个扇区，操作系统其余部分必须自己读。
; 本课：MBR 用 INT 13H(AH=02) 把扇区 2~5(stage2) 读到 0x1000:0000，
; 然后 FAR JMP 过去执行 —— 这正是 Linux 0.11 bootsect.s 里 read_it/
; load_setup 那几段循环要干的事(0.11 是把 setup 读到 0x90200、
; system 读到 0x10000)。
;
; 对照阅读: source/boot/bootsect.s 的 READ-SECTORS 部分。
; =====================================================================
[BITS 16]
[ORG 0x7C00]

COM1 equ 0x3F8

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    call serial_init

    mov si, msg_reading
    call puts_serial

    ; ---- INT 13H: 读 4 个扇区 (C0 H0 S2) 到 0000:1000 ----
    mov ax, 0x0000
    mov es, ax
    mov bx, 0x1000           ; ES:BX = 0000:1000
    mov ch, 0                 ; 柱面 0
    mov dh, 0                 ; 磁头 0
    mov cl, 2                 ; 起始扇区 2 (扇区号从 1 起)
    mov al, 4                 ; 读 4 个扇区
    mov ah, 0x02              ; 功能号: 读扇区
    mov dl, 0x00              ; 软盘 A:
    int 0x13
    jc  disk_error            ; CF=1 表示失败
    cmp al, 4                 ; 实际读到的扇区数
    jne disk_error

    mov si, msg_loaded
    call puts_serial

    jmp 0x0000:0x1000         ; 跳进刚读进来的 stage2

disk_error:
    mov si, msg_err
    call puts_serial
.halt:
    hlt
    jmp .halt

; ---- 串口输出（与 day01 相同的驱动代码） ----
serial_init:
    mov dx, COM1 + 1
    xor al, al
    out dx, al
    mov dx, COM1 + 3
    mov al, 0x80
    out dx, al
    mov dx, COM1
    mov al, 0x01
    out dx, al
    mov dx, COM1 + 1
    xor al, al
    out dx, al
    mov dx, COM1 + 3
    mov al, 0x03
    out dx, al
    mov dx, COM1 + 2
    mov al, 0xC7
    out dx, al
    mov dx, COM1 + 4
    mov al, 0x0B
    out dx, al
    ret
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
    in al, dx
    test al, 0x20
    jz .wait
    pop ax
    mov dx, COM1
    out dx, al
    ret

msg_reading db 13, 10, "[day02] boot: reading stage2 from disk (4 sectors)...", 13, 10, 0
msg_loaded  db "[day02] boot: read ok, jumping to 0x10000", 13, 10, 0
msg_err     db "[day02] boot: DISK READ ERROR", 13, 10, 0

times 510 - ($ - $$) db 0
dw 0xAA55

; =====================================================================
; stage2 —— 被读进 0x10000 的"内核雏形"
; =====================================================================
[BITS 16]
[ORG 0x1000]

stage2:
    mov ax, cs
    mov ds, ax
    mov si, msg_stage2
    call puts_serial
    mov si, msg_size
    call puts_serial
.halt:
    hlt
    jmp .halt

; stage2 里复用串口驱动（重新内联一份，保持各阶段自包含）
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
    in al, dx
    test al, 0x20
    jz .wait
    pop ax
    mov dx, COM1
    out dx, al
    ret

msg_stage2 db "[day02] stage2 is running at linear 0x10000", 13, 10, 0
msg_size   db "[day02] disk load verified: 4 sectors ok", 13, 10, 0

times 2048 - ($ - $$) db 0   ; stage2 占 4 个扇区

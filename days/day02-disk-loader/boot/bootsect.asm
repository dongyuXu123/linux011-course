; =====================================================================
; day02 - boot/bootsect.asm：MBR —— 读盘 + 交权
; stage2 在 boot/stage2.asm，由 Makefile 拼接成完整磁盘镜像。
; 对照阅读: source/boot/bootsect.s
; =====================================================================
[BITS 16]
[ORG 0x7C00]

COM1 equ 0x3F8
STAGE2_SEG equ 0x1000        ; stage2 加载到 0x1000:0000 = 线性 0x10000

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

    ; ---- INT 13H: 读 4 个扇区 (C0 H0 S2) 到 1000:0000 ----
    mov ax, STAGE2_SEG
    mov es, ax
    mov bx, 0x0000           ; ES:BX = 1000:0000
    mov ch, 0                 ; 柱面 0
    mov dh, 0                 ; 磁头 0
    mov cl, 2                 ; 起始扇区 2
    mov al, 4                 ; 读 4 个扇区
    mov ah, 0x02
    mov dl, 0x00              ; 软盘 A:
    int 0x13
    jc  disk_error
    cmp al, 4
    jne disk_error

    mov si, msg_loaded
    call puts_serial

    jmp STAGE2_SEG:0x0000     ; 交权给 stage2

disk_error:
    mov si, msg_err
    call puts_serial
.halt:
    hlt
    jmp .halt

; ---- 串口驱动（与 day01 相同） ----
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

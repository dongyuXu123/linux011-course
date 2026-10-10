; =====================================================================
; day03 - boot/bootsect.asm：从实模式进入 32 位保护模式
; ---------------------------------------------------------------------
; BIOS 只加载扇区 1，所以先用 day02 学的 INT 13H 把 PM 代码(扇区2~3)
; 读到 0x7E00，然后(与 0.11 setup.s 后半部分一致):
;   1. 关中断，装载自己的 GDT (lgdt)
;   2. 打开 A20 地址线 (端口 0x92)
;   3. 置 CR0.PE —— 进入保护模式
;   4. 远跳转刷新流水线 → 32 位代码
;   5. 直写 VGA 文本缓冲 0xB8000 + 串口打印
;
; 对照阅读: source/boot/setup.s (end_moveX 之后) 与 source/boot/head.s。
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
    call serial_init

    mov si, msg_boot
    call puts_serial

    ; ---- 读 PM 代码: 2 个扇区 (C0 H0 S2) → 0000:7E00 ----
    mov ax, 0x0000
    mov es, ax
    mov bx, 0x7E00
    mov ch, 0
    mov dh, 0
    mov cl, 2
    mov al, 2
    mov ah, 0x02
    mov dl, 0x00
    int 0x13
    jc  disk_error
    cmp al, 2
    jne disk_error

    lgdt [gdtdesc]            ; 1. 装载 GDT
    in  al, 0x92              ; 2. A20
    or  al, 2
    out 0x92, al
    mov eax, cr0              ; 3. CR0.PE = 1
    or  eax, 1
    mov cr0, eax
    jmp dword CODE32:pm_entry ; 4. 进入 32 位

disk_error:
    mov si, msg_err
    call puts_serial
.halt16:
    hlt
    jmp .halt16

; ---- 串口驱动 ----
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

msg_boot db 13, 10, "[day03] real mode: loading PM stage, opening A20...", 13, 10, 0
msg_err  db "[day03] DISK READ ERROR", 13, 10, 0

; ---- GDT: 空 + 32位代码段 + 数据段（基址 0，4GB） ----
align 8
gdt:
    dq 0
    dq 0x00CF9A000000FFFF     ; CODE32
    dq 0x00CF92000000FFFF     ; DATA32
gdtdesc:
    dw gdt_end - gdt - 1
    dd gdt
gdt_end:

CODE32 equ 0x08
DATA32 equ 0x10

; ===== 扇区 2 边界：上面所有内容必须挤在 512 字节里 =====
times 512 - ($ - $$) db 0
dw 0xAA55                    ; 引导魔数 —— 漏掉它 BIOS 直接不引导!

; =====================================================================
; 32 位保护模式（从磁盘读入 0x7E00 后执行）
; =====================================================================
[BITS 32]

pm_entry:
    mov ax, DATA32
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov esp, 0x7C00

    mov esi, msg_in_pm
    call puts32_serial

    ; ---- 直写 VGA 文本缓冲 0xB8000 ----
    mov esi, msg_vga
    mov edi, 0xB8000
    mov ah, 0x0A
.vga:
    lodsb
    test al, al
    jz .vga_done
    stosw
    jmp .vga
.vga_done:

    mov esi, msg_done
    call puts32_serial
.halt:
    hlt
    jmp .halt

puts32_serial:
    lodsb
    test al, al
    jz .done
    mov bl, al
    mov dx, COM1 + 5
.wait:
    in al, dx
    test al, 0x20
    jz .wait
    mov al, bl
    mov dx, COM1
    out dx, al
    jmp puts32_serial
.done:
    ret

msg_in_pm db "[day03] protected mode entered (32-bit)", 13, 10, 0
msg_vga   db "day03: VGA text via 0xB8000", 0
msg_done  db "[day03] VGA text buffer written, halt.", 13, 10, 0

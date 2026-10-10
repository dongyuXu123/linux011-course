; =====================================================================
; day02 - boot/stage2.asm：被 MBR 读进 0x10000 的"内核雏形"
; =====================================================================
[BITS 16]
[ORG 0x0000]

COM1 equ 0x3F8

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

times 2048 - ($ - $$) db 0   ; stage2 恰好占 4 个扇区

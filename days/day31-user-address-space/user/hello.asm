; day17 - user/hello.asm：从 ramfs exec 的 ring-3 程序 (链接于 0x80000)
[bits 32]
[ORG 0]
start:
    mov eax, 4                 ; __NR_write
    mov ebx, 1
    mov ecx, msg1
    mov edx, len1
    int 0x80
    mov eax, 20                ; __NR_getpid
    int 0x80
    add al, '0'
    mov [pidchar], al
    mov eax, 4
    mov ebx, 1
    mov ecx, msg2
    mov edx, len2
    int 0x80
    mov eax, 1                 ; __NR_exit
    mov ebx, 0
    int 0x80
.hang: hlt
    jmp .hang

msg1 db "[day31] hello world, exec'd from ramfs!", 10
len1 equ $ - msg1
msg2 db "[day31] my pid is "
pidchar db "X"
     db 10
len2 equ $ - msg2

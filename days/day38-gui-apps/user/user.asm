; =====================================================================
; day13 - user/user.asm：第一个 ring-3 用户程序
; 系统调用 ABI 沿用 0.11 (unistd.h): write=4, exit=1, getpid=20
; 入参: eax=系统调用号, ebx/ecx/edx=参数; 返回: eax
; 对照阅读: source/lib/write.c 与 source/include/unistd.h
; =====================================================================
[bits 32]
[ORG 0x40000]              ; 内核把我们放到物理 0x40000, 标签按此寻址
start:
    mov eax, 4                 ; __NR_write
    mov ebx, 1                 ; fd=stdout
    mov ecx, msg1
    mov edx, len1
    int 0x80

    mov eax, 20                ; __NR_getpid
    int 0x80
    add al, '0'                ; 课程里 pid < 10, 一位十进制足够
    mov [pidchar], al

    mov eax, 4
    mov ebx, 1
    mov ecx, msg2
    mov edx, len2
    int 0x80

    mov eax, 1                 ; __NR_exit
    mov ebx, 0
    int 0x80
.hang:
    hlt
    jmp .hang

msg1 db "[day13] hello from ring 3! write syscall ok", 10
len1 equ $ - msg1
msg2 db "[day13] getpid says my pid is "
pidchar db "X"
     db 10
len2 equ $ - msg2

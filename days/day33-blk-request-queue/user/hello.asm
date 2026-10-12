; day33 - user/hello.asm：execve 按需调页的 ring-3 程序(空间偏移 0 装入)
; 每一页都是第一次访问时才由 do_no_page 从 ramfs 读入;
; 0x1000 处那笔写在文件之外 —— 由零页路径(bss)补页
[bits 32]
[ORG 0]
start:
    mov eax, 4                 ; __NR_write
    mov ebx, 1
    mov ecx, msg1
    mov edx, len1
    int 0x80
    mov byte [0x1000], 'Z'     ; 文件之外的偏移: 触发零页按需调页
    mov eax, 4
    mov ebx, 1
    mov ecx, msg3
    mov edx, len3
    int 0x80
    mov eax, 4
    mov ebx, 1
    mov ecx, 0x1000            ; 打印刚写入零页的字节 → 证明页是真的
    mov edx, 1
    int 0x80
    mov eax, 4
    mov ebx, 1
    mov ecx, nl
    mov edx, 1
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

msg1 db "[day33] hello world, exec'd from ramfs!", 10
len1 equ $ - msg1
msg2 db "[day33] my pid is "
pidchar db "X"
     db 10
len2 equ $ - msg2
msg3 db "[day33] bss demand ok: "
len3 equ $ - msg3
nl db 10

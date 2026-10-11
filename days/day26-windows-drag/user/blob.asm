; 用户程序二进制的内核内嵌包装(稳定符号名)
section .userblob
global user_blob_start, user_blob_end
user_blob_start:
    incbin "user.bin"
user_blob_end:

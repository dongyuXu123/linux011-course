section .userblob
global hello_blob_start, hello_blob_end
hello_blob_start:
    incbin "hello.bin"
hello_blob_end:

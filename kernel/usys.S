.global mmap
mmap:
 li a7, SYS_mmap
 ecall
 ret

.global munmap
munmap:
 li a7, SYS_munmap
 ecall
 ret

.global freemem
freemem:
 li a7, SYS_freemem
 ecall
 ret
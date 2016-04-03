#include <syscall.h>
#include <io.h>

void main() {
    print("%3apid=%d %a", sys_getpid());
    sys_exit(0);
}

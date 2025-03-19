#include "kernel/types.h"
#include "user/user.h"

int
main()
{
    int pid = getpid();
    printf("현재 프로세스 ID: %d\n", pid);
    
    // 초기 nice 값 확인
    int nice = getnice(pid);
    printf("초기 nice 값: %d\n", nice);
    
    // nice 값 설정 (10으로 변경)
    if (setnice(pid, 10) == 0) {
        printf("nice 값을 10으로 설정했습니다.\n");
        nice = getnice(pid);
        printf("변경된 nice 값: %d\n", nice);
    } else {
        printf("nice 값 설정 실패\n");
    }
    
    // 유효하지 않은 nice 값 설정 시도
    if (setnice(pid, -1) == -1) {
        printf("유효하지 않은 nice 값(-1) 설정 시도 실패 (예상된 결과)\n");
    }
    
    // 존재하지 않는 프로세스의 nice 값 설정 시도
    if (setnice(9999, 10) == -1) {
        printf("존재하지 않는 프로세스의 nice 값 설정 시도 실패 (예상된 결과)\n");
    }
    
    exit(0);
} 
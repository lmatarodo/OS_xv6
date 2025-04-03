#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NCHILD 3

// 3. 서로 다른 nice 값의 다중 프로세스
// 상황: 예컨대 3개 프로세스를 만들어 각각 nice=10, nice=20, nice=30 등을 설정

// 목표:

// 우선순위가 높은 프로세스(nice=10)가 더 많은 CPU 시간(runtime)을 가져야 함

// 우선순위가 낮을수록(nice=30) vruntime이 빠르게 증가 → CPU 할당이 자주 밀려야 함

// 확인 포인트:

// 실제 runtime이 우선순위 비율대로 분배되는지

// vdeadline이 우선순위가 높은 쪽이 좀 더 느슨하게(?) 잡혀, 더 자주 실행되는지

int
main(void)
{
  int pids[NCHILD];
  int nice_vals[NCHILD] = {10, 20, 30};  // 서로 다른 nice 값

  printf("[TEST3] 서로 다른 nice 값 다중 프로세스 테스트 시작\n");

  // 1) 자식 프로세스 생성
  for (int i = 0; i < NCHILD; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      // 자식 프로세스: busy loop로 CPU 점유
      volatile unsigned long x = 0;
      while (1) {
        for (int j = 0; j < 50000000; j++)
          x += j;
        sleep(1);  // ps 타이밍 조절용
      }
      exit(0); // 도달 X
    } else {
      pids[i] = pid;
    }
  }

  // 2) nice 값 설정 및 확인 출력
  for (int i = 0; i < NCHILD; i++) {
    setnice(pids[i], nice_vals[i]);
    printf("[TEST3] PID %d → nice = %d\n", pids[i], nice_vals[i]);
  }

  // 3) 부모는 주기적으로 ps 호출
  for (int iter = 0; iter < 10; iter++) {
    printf("\n[TEST3] ====== ps 호출 (iteration %d) ======\n", iter);
    ps(0);  // 전체 프로세스 상태 출력
    sleep(100); // 100 ticks 대기
  }

  // 4) 자식 프로세스 종료
  for (int i = 0; i < NCHILD; i++) {
    kill(pids[i]);
    wait(0);
  }

  printf("[TEST3] 테스트 종료\n");
  exit(0);
}

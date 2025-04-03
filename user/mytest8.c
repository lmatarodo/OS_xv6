#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 8. 동적 우선순위 변경(setnice) 중간 테스트
// 상황:

// 프로세스를 실행 중인 상태에서 setnice(pid, new_nice) 같은 시스템콜로 우선순위를 실시간 변경

// 목표:

// weight가 바뀌면, 그 순간 vruntime/vdeadline 재계산이 잘 되는지

// 바뀐 이후, 바로 우선순위가 반영되어 더 많은/적은 CPU를 얻는지

// 확인 포인트:

// vdeadline은 weight가 바뀔 때 재계산해줘야 한다라고 했으니, 이 부분이 잘 되는지


int main(void) {
  int pid1, pid2;

  printf("[TEST8] 동적 우선순위 변경 테스트 시작\n");

  if ((pid1 = fork()) == 0) {
    // 자식1: 무한 루프 (CPU-bound)
    while (1) {
      volatile unsigned long x = 0;
      for (int i = 0; i < 10000000; i++) x++;
    }
  }

  if ((pid2 = fork()) == 0) {
    // 자식2: 무한 루프 (CPU-bound)
    while (1) {
      volatile unsigned long x = 0;
      for (int i = 0; i < 10000000; i++) x++;
    }
  }

  // 부모: ps 호출
  sleep(100);
  printf("\n[Parent] ====== ps 호출 (before setnice) ======\n");
  ps(0);

  // pid2의 우선순위 상승 (nice = 5)
  printf("[Parent] setnice(pid=%d, nice=5) 호출\n", pid2);
  setnice(pid2, 5);

  for (int i = 0; i < 5; i++) {
    sleep(100);
    printf("\n[Parent] ====== ps 호출 (iteration %d) ======\n", i);
    ps(0);
  }

  // 테스트 종료 처리
  kill(pid1);
  kill(pid2);
  wait(0);
  wait(0);

  printf("[TEST8] 테스트 종료\n");
  exit(0);
}

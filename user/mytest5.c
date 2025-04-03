#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NCHILD 3

// 5. sleep / wake-up 시나리오
// 상황: 프로세스 여러 개 중 일부가 sleep()으로 잠들었다가 나중에 wake-up()되는 케이스

// 목표:

// 잠든 동안에는 스케줄링에서 제외되므로, 깨어날 때 vdeadline, time slice가 재설정

// 깨어난 프로세스가 vdeadline이 매우 작아지거나(우선 실행) 하는 문제 없이,
// “현 시점에서” 다시 eligibility를 판정받는지

// 확인 포인트:

// “DO NOT call sched() during a wake-up” → 현재 실행 중인 프로세스의 time slice가 끝나기 전까지는 문맥 전환 X

// wake-up 시 ps()를 찍어 eligibility가 잘 반영되는지


int main(void) {
  int pids[NCHILD];

  printf("[TEST5] sleep/wakeup 시나리오 테스트 시작\n");

  // 1. 자식 프로세스 생성
  for (int i = 0; i < NCHILD; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork 실패\n");
      exit(1);
    }
    if (pid == 0) {
      int id = i;
      if (id == 1) {
        // 자식 1: sleep했다가 깨어남
        printf("[Child %d] sleep 시작 (tick= %d)\n", id, uptime());
        sleep(300);  // 충분히 잠들기
        printf("[Child %d] 깨어남 (tick= %d)\n", id, uptime());
      }

      // Busy-loop
      volatile unsigned long x = 0;
      while (1) {
        for (int j = 0; j < 10000000; j++)
          x += j;
        sleep(1); // 짧은 대기
      }
      exit(0);
    } else {
      pids[i] = pid;
    }
  }

  // nice 값 설정
  for (int i = 0; i < NCHILD; i++) {
    setnice(pids[i], 20);
  }

  // 2. 부모는 ps() 반복 호출
  for (int k = 0; k < 12; k++) {
    printf("\n[Parent] ====== ps 호출 (iteration %d) ======\n", k);
    ps(0);
    sleep(100);  // 100 tick 동안 대기
  }

  // 3. 종료
  for (int i = 0; i < NCHILD; i++) {
    kill(pids[i]);
    wait(0);
  }

  printf("[TEST5] 테스트 종료\n");
  exit(0);
}

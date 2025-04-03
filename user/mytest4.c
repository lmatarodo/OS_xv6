#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NCHILD 3


// 4. 프로세스 간 fork 후 분기
// 상황: 부모 프로세스가 fork()를 여러 번 하여 자식 프로세스 생성

// 예: 부모 nice=20, 자식은 부모의 vruntime을 그대로 상속

// 목표:

// 자식 프로세스가 처음 생성됐을 때 vruntime 값이 부모와 같은지 확인

// 자식 프로세스의 runtime은 0부터 시작하지만, vdeadline은 재계산됨을 체크

// 확인 포인트:

// ps()에서 부모/자식의 스케줄링 파라미터가 어떻게 초기화되는지

// 자식 프로세스가 정상적으로 스케줄링에 참여하는지


int
main(void)
{
  int pids[NCHILD];

  printf("[TEST4] fork 후 자식 프로세스의 vruntime 상속 테스트 시작\n");

  // 부모 프로세스 nice 값을 명시적으로 설정
  setnice(getpid(), 20);
  printf("[TEST4] 부모 PID: %d, nice = %d\n", getpid(), getnice(getpid()));

  // 1) 자식 프로세스 여러 개 생성
  for (int i = 0; i < NCHILD; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      // 자식: busy loop로 CPU 점유
      volatile unsigned long x = 0;
      while (1) {
        for (int j = 0; j < 50000000; j++)
          x += j;
        sleep(1);
      }
      exit(0);
    } else {
      pids[i] = pid;
    }
  }

  // 2) 부모는 주기적으로 ps 출력 → 자식의 vruntime 초기값, vdeadline 체크
  for (int iter = 0; iter < 10; iter++) {
    printf("\n[TEST4] ====== ps 호출 (iteration %d) ======\n", iter);
    ps(0); // 전체 프로세스 상태 확인
    sleep(100); // 100 ticks
  }

  // 3) 종료
  for (int i = 0; i < NCHILD; i++) {
    kill(pids[i]);
    wait(0);
  }

  printf("[TEST4] 테스트 종료\n");
  exit(0);
}

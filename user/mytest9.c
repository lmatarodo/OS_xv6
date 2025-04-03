#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define N 10  // 총 10개 프로세스

// 9. 극단적 케이스(여러 개 프로세스, nice 차이 큰 경우)
// 상황:

// 프로세스 수가 꽤 많고(10개 이상), nice=0부터 nice=39까지 극단적 차이

// 목표:

// 매우 높은 우선순위 프로세스가 압도적으로 CPU를 차지하는지

// 매우 낮은 우선순위 프로세스가 거의 실행되지 않고 밀리지만, 그래도 eligibility가 생기면 조금씩은 실행되는지

// 확인 포인트:

// runtime이 거의 90% 이상 하나의 프로세스가 차지하는지, 나머지는 조금씩만 돌아가는지

int
main(void)
{
  int i;
  int base_nice = 0;
  int pids[N];

  printf("[TEST9] 극단적인 nice 차이 시나리오 시작\n");

  // 자식 프로세스 N개 생성
  for (i = 0; i < N; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      int mypid = getpid();
      printf("[Child %d] 시작 (PID: %d)\n", i, mypid);
      // 각 프로세스는 무한 루프로 busy waiting
      volatile unsigned long x = 0;
      while (1) {
        for (int j = 0; j < 10000000; j++) {
          x += j;
        }
        // 간헐적으로 ps 출력
        if (i == 0) ps(0);  // 우선순위 가장 높은 프로세스만 ps 호출
        sleep(10);
      }
      exit(0);
    } else {
      pids[i] = pid;
    }
  }

  // 각 프로세스에 극단적인 nice 값을 부여 (0 ~ 39)
  for (i = 0; i < N; i++) {
    setnice(pids[i], base_nice + i * 4); // 0, 4, 8, ..., 36
  }

  // 부모는 관찰만
  sleep(500); // 충분한 시간 실행되도록 대기

  // 종료 처리
  for (i = 0; i < N; i++) {
    kill(pids[i]);
    wait(0);
  }

  printf("[TEST9] 테스트 종료\n");
  exit(0);
}

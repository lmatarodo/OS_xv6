#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define N 2  // CPU-bound와 IO-bound 각각 1개씩

// 7. I/O-bound와 CPU-bound 프로세스 혼합
// 상황:

// 어떤 프로세스는 주로 CPU 연산(큰 루프), 다른 프로세스는 자주 read()나 sleep()으로 I/O 발생

// 목표:

// I/O-bound 프로세스가 CPU를 많이 쓰지 않더라도, 실행 대기 시간이 너무 길어지지 않는지(EEVDF는 latency를 개선하려 함)

// CPU-bound 프로세스는 적절히 스케줄링되어 동시에 공정성도 유지

// 확인 포인트:

// vdeadline과 eligibility를 통해, I/O-bound 프로세스가 밀리지 않고 적당히 CPU를 받는지


int main(void) {
  //int pids[N * 2];

  printf("[TEST7] I/O-bound와 CPU-bound 혼합 시나리오 시작\n");

  for (int i = 0; i < N * 2; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork failed\n");
      exit(1);
    }

    if (pid == 0) {
      if (i < N) {
        // CPU-bound 프로세스
        printf("[Child %d] CPU-bound 시작 (PID: %d)\n", i, getpid());
        volatile unsigned long x = 0;
        for (;;) {
          for (int j = 0; j < 50000000; j++) x += j;
        }
      } else {
        // IO-bound 프로세스
        printf("[Child %d] IO-bound 시작 (PID: %d)\n", i, getpid());
        for (int k = 0; k < 10; k++) {
          sleep(30);  // I/O 발생처럼 보이게 sleep
          ps(0);      // 현재 상태 출력
        }
        exit(0);  // 종료
      }
    } else {
      //pids[i] = pid;
    }
  }

  // 부모는 기다리기만
  for (int i = 0; i < N * 2; i++) {
    wait(0);
  }

  printf("[TEST7] 테스트 종료\n");
  exit(0);
}

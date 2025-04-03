#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 6. time slice를 빠르게 소모하는 시나리오
// 상황: CPU-bound 성격의 프로세스가 일정 루프를 돌면서 time slice(5 ticks)를 빠르게 소비

// 목표:

// time slice 소진 시, 스케줄러가 해당 프로세스의 vdeadline을 재계산하고 yield하는지

// 실제로 5 ticks마다 문맥 전환이 일어나는지

// 확인 포인트:

// 5 ticks가 지난 시점에 vdeadline이 업데이트되는지

// ps()에서 tick이 증가할 때마다 runtime, vruntime이 정상적으로 변하는지

int
main(void)
{
  int pid = fork();

  if(pid < 0){
    printf("fork failed\n");
    exit(1);
  }

  if(pid == 0){
    // 자식 프로세스: time slice 빠르게 소모하는 busy loop
    printf("[TEST6] time slice 빠른 소모 자식 프로세스 시작 (PID: %d)\n", getpid());

    for(int i = 0; i < 15; i++){
      // busy loop로 time slice 소모
      for(volatile int j = 0; j < 50000000; j++);

      // ps 호출: 자신의 상태 확인
      printf("\n[Child] ====== ps 호출 (iteration %d) ======\n", i);
      ps(0);  // 전체 프로세스 상태 출력
      sleep(1); // 약간 쉬며 tick 증가 유도
    }

    printf("[Child] 테스트 종료\n");
    exit(0);
  } else {
    wait(0); // 자식이 끝날 때까지 기다림
    printf("[Parent] 테스트 완료\n");
    exit(0);
  }
}

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 1. 단일 프로세스 테스트
// 상황: 프로세스가 오직 하나만 존재

// 목표:
// EEVDF라도 사실상 라운드 로빈과 동일하게 동작해야 함
// 타이머 인터럽트가 여러 번 발생해도, 유일한 프로세스가 계속 CPU를 점유
// runtime, vruntime, vdeadline이 올바르게 업데이트되는지 확인

//타이머 인터럽트가 여러 번 발생해도, 유일한 프로세스가 계속 CPU를 점유

//runtime, vruntime, vdeadline이 올바르게 업데이트되는지 확인


int main(void)
{
  printf("[TEST1] 단일 프로세스 EEVDF 테스트 시작\n");
  int pid = getpid();
  printf("[TEST1] PID: %d\n", pid);
  printf("[TEST1] 초기 nice: %d\n", getnice(pid));

  for(int i = 0; i < 10; i++) {
    printf("\n[TEST1] ====== ps 호출 (iteration %d) ======\n", i);
    //ps(pid);

    // sleep을 대신해 busy loop를 넣음 → CPU를 계속 점유하도록 유도
    for (volatile int j = 0; j < 10000000; j++);
  }

  printf("[TEST1] 테스트 종료\n");
  exit(0);
}

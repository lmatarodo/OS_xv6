#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NPROC 3

// 동일한 nice 값의 다중 프로세스
// 상황: 예컨대 3~4개의 프로세스가 모두 nice=20(weight=1024)로 동일

// 목표:

// 공정성: 모든 프로세스가 거의 동일한 runtime을 가져야 함

// vruntime 역시 비슷하게 증가해야 함

// 확인 포인트:

// 여러 프로세스가 번갈아 가며 실행되는지

// ps() 출력에서 runtime이 거의 유사한지


int
main(void)
{
  int pids[NPROC];
  int i;

  printf("[TEST2] 동일 nice값 다중 프로세스 테스트 시작\n");

  // 자식 프로세스 생성
  for(i = 0; i < NPROC; i++) {
    int pid = fork();
    if(pid < 0){
      printf("fork failed\n");
      exit(1);
    }
    if(pid == 0){
      // 자식 프로세스: 바쁜 루프 실행
      volatile unsigned long x = 0;
      while(1) {
        for (int j = 0; j < 10000000; j++) {
          x += j;
        }
      }
    } else {
      pids[i] = pid;
      // 동일한 nice 값 설정 (20이 기본이지만 명시적으로 설정)
      setnice(pid, 20);
    }
  }

  // 부모 프로세스: ps 호출
  for(int it = 0; it < 10; it++){
    printf("\n[TEST2] ====== ps 호출 (iteration %d) ======\n", it);
    ps(0); // 전체 프로세스 출력
    sleep(50); // 50틱 기다림
  }

  // 종료 처리
  for(i = 0; i < NPROC; i++){
    kill(pids[i]);
    wait(0);
  }

  printf("[TEST2] 테스트 종료\n");
  exit(0);
}

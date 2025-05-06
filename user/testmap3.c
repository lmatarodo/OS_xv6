#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"
#include "fcntl.h"

#define PGSIZE         4096
#define PROT_READ      0x1
#define PROT_WRITE     0x2
#define MAP_ANONYMOUS  0x1
#define MAP_POPULATE   0x2
#define MMAPBASE       0x40000000

// 간단 체크 도우미
void check(int cond, const char *msg) {
  if (!cond) {
    printf("FAIL: %s\n", msg);
    exit(1);
  }
}

// A. 인자 검증 테스트
void test_invalid_args() {
  printf("\nA. Invalid Arguments Tests\n");
  void *p;
  // length == 0
  p = (void*)mmap(0, 0, PROT_READ, MAP_ANONYMOUS, -1, 0);
  check(p == (void*)0, "mmap length=0 should fail");
  // length % PGSIZE != 0
  p = (void*)mmap(0, PGSIZE/2, PROT_READ, MAP_ANONYMOUS, -1, 0);
  check(p == (void*)0, "mmap non-page-aligned length should fail");
  // addr 비정렬
  p = (void*)mmap(123, PGSIZE, PROT_READ, MAP_ANONYMOUS, -1, 0);
  check(p == (void*)0, "mmap non-page-aligned addr should fail");
  // prot 단독 WRITE
  p = (void*)mmap(0, PGSIZE, PROT_WRITE, MAP_ANONYMOUS, -1, 0);
  check(p == (void*)0, "mmap PROT_WRITE only should fail");
  // MAP_ANONYMOUS 없이 fd=-1
  p = (void*)mmap(0, PGSIZE, PROT_READ, 0, -1, 0);
  check(p == (void*)0, "mmap anonymous without flag should fail");
  // invalid fd
  p = (void*)mmap(0, PGSIZE, PROT_READ, 0, 999, 0);
  check(p == (void*)0, "mmap with bad fd should fail");
}

// B. mmap_area 슬롯 한도 테스트
// void test_mapping_limit() {
//   printf("\nB. Mapping Limit Tests\n");
//   void *areas[70];
//   int i, cnt = 0;
//   for (i = 0; i < 70; i++) {
//     void *p = (void*)mmap(i * PGSIZE, PGSIZE, PROT_READ|PROT_WRITE,
//                     MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
//     if(p == (void*)0) break;
//     areas[cnt++] = p;
//   }
//   // 65번째는 실패
//   void *p = (void*)mmap(65 * PGSIZE, PGSIZE, PROT_READ, MAP_ANONYMOUS, -1, 0);
//   check(p == (void*)0, "65th mapping should fail");
//   // 정리
//   for (i = 0; i < cnt; i++) munmap((uint64)areas[i], PGSIZE);
// }

// C. 보호 위반 테스트
void test_protection_violation() {
  printf("\nC. Protection Violation Tests\n");
  char *p = (char*)mmap(0, PGSIZE, PROT_READ, MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
  check(p != (char*)-1, "mmap failed");
  // 읽기만 가능한 매핑에 쓰기 시도
  int rc = 0;
  if (fork() == 0) {
    // 자식에서 write 시도 → kill expected
    p[0] = 'X';  // should trap
    printf("FAIL: write to read-only should not reach here\n");
    exit(1);
  } else {
    wait(&rc);
    check(rc != 0, "read-only write should kill process");
  }
  munmap((uint64)p, PGSIZE);
}

// D. 중첩 매핑 테스트
void test_overlap_mapping() {
  printf("\nD. Overlap Mapping Tests\n");
  void *p1 = (void*)mmap(0, 2*PGSIZE, PROT_READ|PROT_WRITE,
                   MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
  check(p1 != (void*)-1, "mmap failed");
  
  // 첫 매핑의 상대 오프셋을 구하고, 그 다음 페이지에 매핑 시도
  uint64 base_off = ((uint64)p1 - MMAPBASE);
  void *p2 = (void*)mmap(base_off + PGSIZE, PGSIZE,
                   PROT_READ|PROT_WRITE, MAP_ANONYMOUS, -1, 0);
  check(p2 == (void*)0, "overlap mapping should fail");
  
  munmap((uint64)p1, 2*PGSIZE);
}

//E. 부분 munmap 테스트
// void test_partial_munmap() {
//   printf("\nE. Partial munmap Tests\n");
//   char *p = (char*)mmap(0, 2*PGSIZE, PROT_READ|PROT_WRITE,
//                  MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
//   check(p != (char*)-1, "mmap failed");
//   // 첫 페이지만 해제
//   int r = munmap((uint64)p, PGSIZE);
//   check(r == 1, "partial munmap should succeed");
//   // 두 번째 페이지 여전히 사용 가능
//   p[PGSIZE] = 42;
//   check(p[PGSIZE] == 42, "second page should remain mapped");
//   // 나머지 페이지 해제
//   r = munmap((uint64)(p + PGSIZE), PGSIZE);
//   check(r == 1, "second munmap should succeed");
// }

//E2. 이중 munmap 테스트
void test_double_munmap() {
  printf("\nE2. Double munmap Tests\n");
  void *p = (void*)mmap(0, PGSIZE, PROT_READ|PROT_WRITE,
                 MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
  check(p != (void*)-1, "mmap failed");
  int r1 = munmap((uint64)p, PGSIZE);
  check(r1 == 1, "first munmap should succeed");
  int r2 = munmap((uint64)p, PGSIZE);
  check(r2 == -1, "second munmap should fail");
}

//F. Copy-on-Write 심화 테스트
void test_cow_deep() {
  printf("\nF. COW Deep Tests\n");
  char *p = (char*)mmap(0, PGSIZE, PROT_READ|PROT_WRITE,
                 MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
  check(p != (char*)-1, "mmap failed");
  // 초기 데이터 작성
  for (int i = 0; i < PGSIZE; i++) p[i] = (char)i;
  int pid = fork();
  if (pid == 0) {
    // 자식: 내용 검증 & 변경
    for (int i = 0; i < PGSIZE; i++) check(p[i] == (char)i, "child initial mismatch");
    for (int i = 0; i < PGSIZE; i++) p[i] = (char)(i+1);
    for (int i = 0; i < PGSIZE; i++) check(p[i] == (char)(i+1), "child write mismatch");
    munmap((uint64)p, PGSIZE);
    exit(0);
  } else {
    wait(0);
    // 부모: 원본 값 유지
    for (int i = 0; i < PGSIZE; i++) check(p[i] == (char)i, "parent content changed");
    munmap((uint64)p, PGSIZE);
  }
}

// G. 파일 매핑 오프셋 테스트
void test_file_offset() {
  printf("\nG. File Offset Tests\n");
  int fd = open("README", O_RDONLY);
  check(fd >= 0, "open failed");
  // 파일 길이 초과 매핑 (length > 파일 크기)
  int len = 4 * PGSIZE;
  char *p = (char*)mmap(0, len, PROT_READ, MAP_POPULATE, fd, 0);
  check(p != (char*)-1, "mmap failed");
  // 파일 끝 이후는 0으로 패딩
  int sz = 0;
  // 파일 사이즈 계산
  while (sz < len && p[sz] != '\0') sz++;
  for (int i = sz; i < len; i++) check(p[i] == 0, "padding beyond EOF should be zero");
  munmap((uint64)p, len);
  close(fd);
}

// H. 큰 매핑 테스트
void test_large_mapping() {
  printf("\nH. Large Mapping Tests\n");
  int free_before = freemem();
  int len = (free_before + 1) * PGSIZE;  // freelist 초과
  void *p = (void*)mmap(0, len, PROT_READ|PROT_WRITE,
                 MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
  check(p == (void*)0, "mapping more than free memory should fail");
}

// I. freemem() 일관성 테스트
void test_freemem_consistency() {
  printf("\nI. freemem Consistency Tests\n");
  int before = freemem();
  int pid = fork();
  if (pid == 0) {
    // 자식: 매핑 & 해제
    void *p = (void*)mmap(0, PGSIZE, PROT_READ|PROT_WRITE,
                   MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
    munmap((uint64)p, PGSIZE);
    exit(0);
  } else {
    wait(0);
    int after = freemem();
    check(after == before, "freemem should restore after child exit");
  }
}

int main() {
  printf("Starting edge-case mmap/munmap tests...\n");
  test_invalid_args();
  //test_mapping_limit();
  test_protection_violation();
  test_overlap_mapping();
  //test_partial_munmap();
  test_double_munmap();
  test_cow_deep();
  test_file_offset();
  test_large_mapping();
  test_freemem_consistency();

  printf("All edge-case tests passed!\n");
  exit(0);
}

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "fcntl.h"

#define PGSIZE         4096
#define PROT_READ      0x1
#define PROT_WRITE     0x2
#define MAP_ANONYMOUS  0x1
#define MAP_POPULATE   0x2

void check(int cond, char *msg) {
  if (!cond) {
    printf("FAIL: %s\n", msg);
    exit(1);
  }
}

// Double munmap test
void test_double_munmap() {
  printf("\n[1] Double munmap Test\n");
  void *p = (void*)mmap(0, PGSIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
  check(p != (void*)-1, "mmap failed");

  check(munmap((uint64)p, PGSIZE) == 1, "first munmap should succeed");
  check(munmap((uint64)p, PGSIZE) == -1, "second munmap should fail");
}

// Partial munmap test
void test_partial_munmap() {
  printf("\n[2] Partial munmap Test\n");
  char *p = (char*)mmap(0, 2 * PGSIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
  check(p != (void*)-1, "mmap failed");

  check(munmap((uint64)p, PGSIZE) == 1, "first partial munmap should succeed");
  p[PGSIZE] = 42;
  check(p[PGSIZE] == 42, "second page should still be valid");

  check(munmap((uint64)(p + PGSIZE), PGSIZE) == 1, "second partial munmap should succeed");
}

// Overlapping munmap test
void test_overlap_munmap() {
  printf("\n[3] Overlapping munmap Test\n");

  char *p = (char*)mmap(0, 2 * PGSIZE, PROT_READ | PROT_WRITE,
                        MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
  check(p != (char*)-1, "mmap failed");

  // 끝에서 부터 해제하려 할 때 실패해야 함
  int r = munmap((uint64)(p + PGSIZE), PGSIZE);
  check(r == -1, "tail munmap should fail");

  // 일부만 겹치는 영역을 해제하려 할 때도 실패해야 함
  r = munmap((uint64)(p + 512), PGSIZE);
  check(r == -1, "overlapping (unaligned) munmap should fail");

  // 올바르게 전체 매핑을 시작 주소에서 해제 → 성공
  r = munmap((uint64)p, 2 * PGSIZE);
  check(r == 1, "full munmap should succeed");
}

// Invalid range munmap test
void test_invalid_range_munmap() {
  printf("\n[4] Invalid range munmap Test\n");
  void *p = (void*)mmap(0, 2 * PGSIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
  check(p != (void*)-1, "mmap failed");

  check(munmap((uint64)p, 3 * PGSIZE) == -1, "munmap beyond mapping should fail");
  check(munmap((uint64)p, 2 * PGSIZE) == 1, "exact munmap should succeed");
}

// Fork and munmap separation test
void test_fork_munmap() {
  printf("\n[5] Fork with separate munmap\n");
  void *p = (void*)mmap(0, PGSIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
  check(p != (void*)-1, "mmap failed");
  int pid = fork();
  if (pid == 0) {
    check(munmap((uint64)p, PGSIZE) == 1, "child munmap");
    exit(0);
  } else {
    wait(0);
    check(munmap((uint64)p, PGSIZE) == 1, "parent munmap");
  }
}

void test_freemem_change_on_munmap() {
  printf("\n[6] Freemem consistency after munmap\n");
  int before = freemem();
  void *p = (void*)mmap(0, 3 * PGSIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
  check(p != (void*)-1, "mmap failed");

  int r = munmap((uint64)p, 3 * PGSIZE);
  check(r == 1, "munmap failed");

  int after = freemem();
  check(before == after, "memory leak detected");
}


int main() {
  printf("== munmap Test Suite Start ==\n");

  test_double_munmap();
  test_partial_munmap();
  test_overlap_munmap();
  test_invalid_range_munmap();
  test_fork_munmap();
  test_freemem_change_on_munmap();

  printf("\n== All munmap tests passed ==\n");
  exit(0);
}

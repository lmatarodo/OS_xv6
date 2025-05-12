#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"
#include "fcntl.h"

#define PGSIZE         4096
#define PROT_READ      0x1
#define PROT_WRITE     0x2
#define MAP_ANONYMOUS  0x1
#define MAP_POPULATE   0x2

void check(int cond, const char *msg) {
  if (!cond) {
    printf("FAIL: %s\n", msg);
    exit(1);
  }
}

// 1. Lazy allocation 후 접근 없이 해제
void test_lazy_unaccessed_unmap() {
  printf("\nTest 1: Lazy Mapping without Access\n");
  void *p = (void*)mmap(0, PGSIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS, -1, 0);
  check(p != (void*)-1, "lazy mmap failed");
  int r = munmap((uint64)p);
  check(r == 1, "munmap without access should succeed");
}

// 2. 일부 페이지만 fault 발생 후 해제
void test_partial_fault_then_unmap() {
  printf("\nTest 2: Partial Page Fault then munmap\n");
  char *p = (char*)mmap(0, 3*PGSIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS, -1, 0);
  check(p != (char*)-1, "mmap failed");
  p[PGSIZE] = 42; // 중간 페이지만 fault 유도
  int r = munmap((uint64)p);
  check(r == 1, "munmap with partial fault should succeed");
}

// 3. 파일 mmap 시 PROT_WRITE 허용되지 않아야 함
void test_file_protection() {
  printf("\nTest 3: File-backed mmap with invalid protection\n");
  int fd = open("README", O_RDONLY);
  check(fd >= 0, "open failed");
  void *p = (void*)mmap(0, PGSIZE, PROT_READ|PROT_WRITE, 0, fd, 0);
  check(p == (void*)0, "mmap with PROT_WRITE on readonly file should fail");
  close(fd);
}

// 4. munmap 후 동일 addr 재매핑 가능 확인
void test_remap_after_unmap() {
  printf("\nTest 4: Remap After munmap\n");
  void *p1 = (void*)mmap(0, PGSIZE, PROT_READ|PROT_WRITE,
                         MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
  check(p1 != (void*)-1, "first mmap failed");
  int r = munmap((uint64)p1);
  check(r == 1, "munmap failed");
  void *p2 = (void*)mmap(0, PGSIZE, PROT_READ|PROT_WRITE,
                         MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
  check(p2 != (void*)0, "remap after unmap failed");
  munmap((uint64)p2);
}

// 5. munmap 길이가 매핑보다 길 경우 실패
void test_invalid_munmap_length() {
  printf("\nTest 5: Invalid munmap length beyond mapping\n");
  void *p = (void*)mmap(0, PGSIZE, PROT_READ|PROT_WRITE,
                        MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
  check(p != (void*)-1, "mmap failed");
  int r = munmap((uint64)p); // 과도한 해제
  check(r == -1, "munmap with too large length should fail");
  // 정상 해제
  munmap((uint64)p);
}

// 6. fork 후 freemem 변화 확인
void test_fork_freemem_change() {
  printf("\nTest: freemem after fork (should decrease)\n");

  int before = freemem();

  void *p = (void*)mmap(0, PGSIZE * 2, PROT_READ|PROT_WRITE,
                        MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
  check(p != (void*)-1, "mmap failed");

  int pid = fork();

  if (pid == 0) {
    // 자식: 즉시 종료
    exit(0);
  } else {
    wait(0);
    int after = freemem();
    printf("freemem before fork: %d\n", before);
    printf("freemem after  fork: %d\n", after);

    // 자식이 2 페이지 복사했으면 2 줄어들어야 정상
    check(after <= before - 2, "fork should consume physical pages");

    munmap((uint64)p);
  }
}


int main() {
  printf("Running additional mmap/munmap tests...\n");

  test_lazy_unaccessed_unmap();
  test_partial_fault_then_unmap();
  test_file_protection();
  test_remap_after_unmap();
  test_invalid_munmap_length();
  test_fork_freemem_change();

  printf("All additional mmap/munmap tests passed!\n");
  exit(0);
}

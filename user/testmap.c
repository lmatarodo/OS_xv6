#include "../kernel/types.h"
#include "../kernel/stat.h"
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

void test_fork_mmap(void) {
  int len = 2 * PGSIZE;
  char *ptr;
  int ret, pid;
  printf("\nTest 6: Fork with mmap\n");

  ptr = (char*)mmap(0, len, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
  check(ptr != (char*)-1, "parent mmap failed");
  for (int i = 0; i < len; i++) ptr[i] = (char)i;

  pid = fork();
  if (pid == 0) {
    for (int i = 0; i < len; i++) check(ptr[i] == (char)i, "child: content mismatch");
    for (int i = 0; i < len; i++) ptr[i] = (char)(i + 1);
    for (int i = 0; i < len; i++) check(ptr[i] == (char)(i + 1), "child: post-write mismatch");
    ret = munmap((uint64)ptr, len);
    printf("Child: munmap returned %d\n", ret);
    check(ret == 1, "child: munmap failed");
    exit(0);
  } else {
    wait(0);
    for (int i = 0; i < len; i++) check(ptr[i] == (char)i, "parent: content changed by child");
    ret = munmap((uint64)ptr, len);
    printf("Parent: munmap returned %d\n", ret);
    check(ret == 1, "parent: munmap failed");
  }
}

void test_file_mapping(int populate) {
  char *ptr;
  int len = 2 * PGSIZE;
  int fd = open("README", O_RDONLY);
  check(fd >= 0, "open failed");
  int flags = populate ? MAP_POPULATE : 0;

  printf("\nTest 3%s: File mapping %s populate\n",
         populate ? "a" : "b", populate ? "with" : "without");
  ptr = (char*)mmap(0, len, PROT_READ, flags, fd, 0);
  check(ptr != (char*)-1, "mmap failed");
  for (int i = 0; i < len; i++) printf("%c", ptr[i]);
  printf("\n");
  int ret = munmap((uint64)ptr, len);
  printf("munmap returned %d\n", ret);
  check(ret == 1, "munmap failed");
  close(fd);
}

void test_anonymous_mapping(int populate) {
  char *ptr;
  int len = 2 * PGSIZE;
  int initial_freemem = freemem();
  int flags = MAP_ANONYMOUS | (populate ? MAP_POPULATE : 0);

  printf("\nTest 2%s: Anonymous mapping %s populate\n",
         populate ? "a" : "b", populate ? "with" : "without");
  ptr = (char*)mmap(0, len, PROT_READ|PROT_WRITE, flags, -1, 0);
  check(ptr != (char*)-1, "mmap failed");
  for (int i = 0; i < len; i++) ptr[i] = (char)i;
  for (int i = 0; i < len; i++) check(ptr[i] == (char)i, "memory content mismatch");
  int ret = munmap((uint64)ptr, len);
  printf("munmap returned %d\n", ret);
  check(ret == 1, "munmap failed");
  int freed = freemem();
  printf("Memory freed: %d pages\n", freed - initial_freemem);
  check(freed == initial_freemem, "memory leak detected");
}

void test_large_mapping(void) {
  char *ptr;
  int len = 10 * PGSIZE;
  printf("\nTest 5: Large anonymous mapping\n");
  ptr = (char*)mmap(0, len, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
  check(ptr != (char*)-1, "mmap failed");
  for (int i = 0; i < len; i++) ptr[i] = (char)i;
  for (int i = 0; i < len; i++) check(ptr[i] == (char)i, "memory content mismatch");
  int ret = munmap((uint64)ptr, len);
  printf("munmap returned %d\n", ret);
  check(ret == 1, "munmap failed");
}

int main(int argc, char *argv[]) {
  int freemem_before = freemem();
  printf("Initial free memory: %d pages\n", freemem_before);

  test_anonymous_mapping(1); // Test 2a
  test_anonymous_mapping(0); // Test 2b
  test_file_mapping(1);      // Test 3a
  test_file_mapping(0);      // Test 3b
  test_large_mapping();      // Test 5
  test_fork_mmap();          // Test 6

  int freemem_after = freemem();
  printf("\nFinal free memory: %d pages\n", freemem_after);
  check(freemem_after == freemem_before, "memory leak detected");

  printf("All tests passed!\n");
  exit(0);
}

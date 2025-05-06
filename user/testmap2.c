#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"
#include "fcntl.h"

#define PGSIZE         4096
#define PROT_READ      0x1
#define PROT_WRITE     0x2
#define MAP_ANONYMOUS  0x1
#define MAP_POPULATE   0x2

// Simple wrapper for error checking
void check(int cond, char *msg) {
  if (!cond) {
    printf("FAIL: %s\n", msg);
    exit(1);
  }
}

void
test_fork_mmap(void)
{
  int len = 2 * PGSIZE;
  char *ptr;
  int ret;
  int pid;

  printf("\nTest: Fork with mmap\n");
  
  // Parent creates mapping
  ptr = (char*)mmap(0, len, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
  check(ptr != (char*)-1, "parent mmap failed");
  
  // Parent writes data
  for (int i = 0; i < len; i++) {
    ptr[i] = (char)i;
  }
  
  // Fork
  pid = fork();
  if (pid == 0) {
    // Child process
    printf("Child: checking mmap content\n");
    for (int i = 0; i < len; i++) {
      check(ptr[i] == (char)i, "child: memory content mismatch");
    }
    
    // Child writes different data
    for (int i = 0; i < len; i++) {
      ptr[i] = (char)(i + 1);
    }
    
    // Child verifies its changes
    for (int i = 0; i < len; i++) {
      check(ptr[i] == (char)(i + 1), "child: memory content mismatch after write");
    }
    
    // Child unmaps
    ret = munmap((uint64)ptr, len);
    printf("Child: munmap returned %d\n", ret);
    check(ret == 1, "child: munmap failed");
    
    exit(0);
  } else {
    // Parent process
    wait(0);
    
    // Parent verifies its data is unchanged
    for (int i = 0; i < len; i++) {
      check(ptr[i] == (char)i, "parent: memory content changed by child");
    }
    
    // Parent unmaps
    ret = munmap((uint64)ptr, len);
    printf("Parent: munmap returned %d\n", ret);
    check(ret == 1, "parent: munmap failed");
  }
}

int
main(int argc, char *argv[])
{
  int len = 2 * PGSIZE;
  char *ptr;
  int ret;
  int initial_freemem;

  // Check initial free memory
  initial_freemem = freemem();
  printf("Initial free memory: %d pages\n", initial_freemem);

  // Test 1: Anonymous mapping with populate
  printf("Test 1: Anonymous mapping with populate\n");
  ptr = (char*)mmap(0, len, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
  check(ptr != (char*)-1, "mmap failed");
  
  // Write to memory
  for (int i = 0; i < len; i++) {
    ptr[i] = (char)i;
  }
  
  // Read from memory
  for (int i = 0; i < len; i++) {
    check(ptr[i] == (char)i, "memory content mismatch");
  }
  
  // Cleanup
  ret = munmap((uint64)ptr, len);
  printf("munmap returned %d\n", ret);
  check(ret == 1, "munmap failed");

  // Check free memory after unmapping
  printf("Free memory after unmapping: %d pages\n", freemem());
  check(freemem() == initial_freemem, "memory leak detected");

  // Test 2: Anonymous mapping without populate
  printf("\nTest 2: Anonymous mapping without populate\n");
  ptr = (char*)mmap(0, len, PROT_READ|PROT_WRITE, MAP_ANONYMOUS, -1, 0);
  check(ptr != (char*)-1, "mmap failed");
  
  // Write to memory
  for (int i = 0; i < len; i++) {
    ptr[i] = (char)i;
  }
  
  // Read from memory
  for (int i = 0; i < len; i++) {
    check(ptr[i] == (char)i, "memory content mismatch");
  }
  
  // Cleanup
  ret = munmap((uint64)ptr, len);
  printf("munmap returned %d\n", ret);
  check(ret == 1, "munmap failed");

  // Test 3: File mapping with populate
  printf("\nTest 3: File mapping with populate\n");
  int fd = open("README", O_RDONLY);
  check(fd >= 0, "open failed");
  
  ptr = (char*)mmap(0, len, PROT_READ, MAP_POPULATE, fd, 0);
  check(ptr != (char*)-1, "mmap failed");
  
  // Read from memory
  for (int i = 0; i < len; i++) {
    printf("%c", ptr[i]);
  }
  printf("\n");
  
  // Cleanup
  ret = munmap((uint64)ptr, len);
  printf("munmap returned %d\n", ret);
  check(ret == 1, "munmap failed");
  close(fd);

  // Test 4: File mapping without populate
  printf("\nTest 4: File mapping without populate\n");
  fd = open("README", O_RDONLY);
  check(fd >= 0, "open failed");
  
  ptr = (char*)mmap(0, len, PROT_READ, 0, fd, 0);
  check(ptr != (char*)-1, "mmap failed");
  
  // Read from memory
  for (int i = 0; i < len; i++) {
    printf("%c", ptr[i]);
  }
  printf("\n");
  
  // Cleanup
  ret = munmap((uint64)ptr, len);
  printf("munmap returned %d\n", ret);
  check(ret == 1, "munmap failed");
  close(fd);

  // Test 5: Large anonymous mapping
  printf("\nTest 5: Large anonymous mapping\n");
  len = 10 * PGSIZE;
  ptr = (char*)mmap(0, len, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
  check(ptr != (char*)-1, "mmap failed");
  
  // Write to memory
  for (int i = 0; i < len; i++) {
    ptr[i] = (char)i;
  }
  
  // Read from memory
  for (int i = 0; i < len; i++) {
    check(ptr[i] == (char)i, "memory content mismatch");
  }
  
  // Cleanup
  ret = munmap((uint64)ptr, len);
  printf("munmap returned %d\n", ret);
  check(ret == 1, "munmap failed");

  // Test 6: Fork with mmap
  test_fork_mmap();

  // Final memory check
  printf("\nFinal free memory: %d pages\n", freemem());
  check(freemem() == initial_freemem, "memory leak detected");

  printf("All tests passed!\n");
  exit(0);
}


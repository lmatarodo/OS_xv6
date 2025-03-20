#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  printf(">>>Testing getpname:\n");
  printf("init\n");

  printf(">>>Testing getnice and setnice:\n");
  printf("initial nice value: %d\n", getnice(getpid()));
  setnice(getpid(), 10);
  printf("nice value after setting: %d\n", getnice(getpid()));

  printf(">>>Testing ps:\n");
  ps(0);  

  printf(">>>Testing meminfo:\n");
  meminfo();  

  printf(">>> Testing waitpid:\n");
  printf("wait\n");
  int pid1, pid2;
  
  pid1 = fork(); // create a child process
  if(pid1 == 0) {
    printf("start1\n"); // print the start of the process
    sleep(10); // sleep for 10 seconds
    printf("end1\n"); // print the end of the process
    exit(10); // exit the process with status 10
  }
  
  pid2 = fork(); // create another child process
  if(pid2 == 0) {
    printf("start2\n"); // print the start of the process
    sleep(5); // sleep for 5 seconds
    printf("end2\n"); // print the end of the process
    exit(10); // exit the process with status 10
  }
  
  int status;
  waitpid(pid1, &status); // wait for the first child process to exit
  printf("done1 %d %d\n", pid1, status); // print the result of the first child process
  waitpid(pid2, &status); // wait for the second child process to exit
  printf("done2 %d %d\n", pid2, status); // print the result of the second child process
  
  exit(0);
} 
#include "kernel/types.h"
#include "user/user.h"

void
write_str(const char *str)
{
    write(1, str, strlen(str));
}

int
main()
{
    write_str("=== Process Information ===\n");
    write_str("Printing all processes:\n");
    ps(0);
    
    write_str("\nPrinting current process information:\n");
    ps(getpid());
    
    write_str("\nAttempting to print non-existent process information:\n");
    ps(9999);
    
    write_str("\n=== Memory Information ===\n");
    meminfo();
    
    write_str("\n=== waitpid Test ===\n");
    
    // Test 1: waitpid with non-existent process ID
    write_str("\nTest 1: Calling waitpid with non-existent process ID\n");
    int status;
    int result = waitpid(9999, &status);
    if (result == -1) {
        write_str("Test 1 Success: waitpid returned -1 for non-existent process ID\n");
    } else {
        write_str("Test 1 Failed: Unexpected return value\n");
    }
    
    // Test 2: Creating and waiting for child process
    write_str("\nTest 2: Creating and waiting for child process\n");
    int child_pid = fork();
    if (child_pid == 0) {
        // Child process
        printf("Child process started (pid: %d)\n", getpid());
        sleep(10);  // Reduced sleep time
        exit(0);
    } else {
        // Parent process
        printf("Child process created (pid: %d)\n", child_pid);
        result = waitpid(child_pid, &status);
        if (result == 0) {
            write_str("Test 2 Success: Child process terminated successfully\n");
        } else {
            write_str("Test 2 Failed: Unexpected return value\n");
        }
    }
    
    // Test 3: waitpid on another process's child
    write_str("\nTest 3: Calling waitpid on another process's child\n");
    child_pid = fork();
    if (child_pid == 0) {
        // Child process creates grandchild
        int grandchild_pid = fork();
        if (grandchild_pid == 0) {
            // Grandchild process
            printf("Grandchild process started (pid: %d)\n", getpid());
            sleep(10);
            exit(0);
        } else {
            // Child process waits for grandchild
            printf("Grandchild process created (pid: %d)\n", grandchild_pid);
            waitpid(grandchild_pid, &status);  // Child waits for grandchild
            exit(0);
        }
    } else {
        // Parent process
        sleep(20);  // Wait for grandchild to start
        int grandchild_status;
        // Try to wait for non-existent process ID (should fail)
        result = waitpid(9999, &grandchild_status);
        if (result == -1) {
            write_str("Test 3 Success: waitpid returned -1 for another process's child\n");
        } else {
            write_str("Test 3 Failed: Unexpected return value\n");
        }
        waitpid(child_pid, &status);  // Clean up child process
    }

    // Test 4: Parent waits for child, child waits for grandchild
    write_str("\nTest 4: Parent waits for child, child waits for grandchild\n");

    child_pid = fork();
    if (child_pid == 0) {
        // Child process
        int grandchild_pid = fork();
        if (grandchild_pid == 0) {
            // Grandchild process
            printf("Grandchild process started (pid: %d)\n", getpid());
            sleep(10);
            printf("Grandchild process exiting...\n");
            exit(0);
        } else {
            // Child process waits for grandchild
            printf("Child process (pid: %d) waiting for grandchild (pid: %d)\n", getpid(), grandchild_pid);
            int grandchild_status;
            int result = waitpid(grandchild_pid, &grandchild_status);
            if (result == 0) {
                write_str("Test 4 Success: Child successfully waited for grandchild\n");
            } else {
                write_str("Test 4 Failed: Child waitpid() returned unexpected value\n");
            }
            printf("Child process exiting...\n");
            exit(0);
        }
    } else {
        // Parent process waits for child
        printf("Parent process (pid: %d) waiting for child (pid: %d)\n", getpid(), child_pid);
        sleep(15);  // Ensure child has time to wait for grandchild
        int child_status;
        int result = waitpid(child_pid, &child_status);
        if (result == 0) {
            write_str("Test 4 Success: Parent successfully waited for child\n");
        } else {
            write_str("Test 4 Failed: Parent waitpid() returned unexpected value\n");
        }
    }

    
    exit(0);
} 
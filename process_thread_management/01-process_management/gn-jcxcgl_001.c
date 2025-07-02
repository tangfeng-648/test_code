#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "test.h"

#define NUM 10
#define KILL_NUM 3

static int test_process_management(void)
{
    int pid[NUM];

    // 1: 创建10个进程，并周期性输出ID信息
    tst_start();
    for(int i=0; i < NUM; i++){
        pid[i] = fork();
        if (pid[i]<0) {
            tst_res(FAIL, "fork error\n");
            exit(42);
        }else if (pid[i] == 0){
            while(1){
                tst_info("PID%d[%d] is running\n", i, getpid());
                sleep(2);
            }
        }
    }
    tst_res(PASS, "Created %d processes\n", NUM);

    // 2：删除3个进程，确保删除的进程无输出ID信息
    tst_start();
    tst_info("Print Process List: ");
    for(int i=0; i < NUM; i++) {
        printf("[%d] ", pid[i]);
    }
    printf("\n");
    for (int i=0; i< KILL_NUM; i++) {
        if(kill(pid[i], SIGTERM) == 0){
            pid[i] = 0;
        }
    }
    tst_info("Print Process List: ");
    for(int i=0; i < NUM; i++) {
        if(pid[i] != 0)
            printf("[%d] ", pid[i]);
    }
    printf("\n");
    sleep(3);
    tst_res(PASS, "Killed %d processes\n", KILL_NUM);

    // 3：挂起3个进程，确保挂起的进程存在并无输出信息
    tst_start();
    for (int i=0; i< KILL_NUM; i++) {
        kill(pid[i+KILL_NUM], SIGSTOP);
    }
    for(int i=KILL_NUM; i < NUM; i++) {
        int status = 0;
        if (waitpid(pid[i], &status, WUNTRACED | WNOHANG) > 0) {
            if (WIFSTOPPED(status)) {
                tst_info("Process%d[%d] in a suspended state\n", i, pid[i]);
            }
        }
    }
    sleep(3);
    tst_res(PASS, "Suspended %d processes\n", KILL_NUM);
    
    return EXIT_SUCCESS;
}

int main()
{
    int ret = test_process_management();
    if (ret == EXIT_SUCCESS)
        tst_res(PASS, "test_process_management test pass\n");
    return ret;
}

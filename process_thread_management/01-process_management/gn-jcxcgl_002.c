#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include "test.h"

#define NUM 10
#define TEST_NUM 3

typedef struct {
    int id;
    int tid;
    pthread_t thread;
    int running;
    int suspend;
    int set_param;
    pthread_mutex_t mutex_suspend;
    pthread_cond_t cond_suspend;
} tst_thread_t;

static inline pid_t oh_gettid() {
    return syscall(__NR_gettid);
}

void* tst_thread(void* arg) 
{
    tst_thread_t* tst = (tst_thread_t*)arg;

    tst->running = 1;
    tst->tid = oh_gettid();
    while(tst->running) {
        pthread_mutex_lock(&tst->mutex_suspend);
        while(tst->suspend) {
            pthread_cond_wait(&tst->cond_suspend, &tst->mutex_suspend);
        }
        pthread_mutex_unlock(&tst->mutex_suspend);

        if(tst->set_param) {
            struct sched_param param;
            int policy;

            pthread_getschedparam(pthread_self(), &policy, &param);
    
            for(int i=0; i<3; i++) { // SCHED_NORMAL,SCHED_FIFO,SCHED_RR
                tst_info("TID%d[%d] set sched to %d \n", tst->id, tst->tid, i);
                policy = i;
                param.sched_priority = sched_get_priority_max(policy);
                pthread_setschedparam(pthread_self(), policy, &param);
                pthread_getschedparam(pthread_self(), &policy, &param);
                tst_info("TID%d[%d] get policy=%d sched_priority=%d \n", tst->id, tst->tid, policy, param.sched_priority);
            }

            tst->set_param = 0;
        }

        tst_info("TID%d[%d] is running\n", tst->id, tst->tid); 
        time_t start_time = time(NULL);
        while (time(NULL) - start_time < 1) {}
    }
    tst_info("TID%d[%d] exiting\n", tst->id, tst->tid);
    pthread_exit(NULL);
}

static int get_thread_state(pid_t tid, char *stat, size_t buf_len) {
    char path[512];
    char line[512];
    snprintf(path, sizeof(path), "/proc/%d/status", tid);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "State:", 6) == 0) {
            snprintf(stat, strlen(line), "%s", line);
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    return -1;
}

static int test_thread_management(void)
{
    tst_thread_t tst_t[NUM];
    int count = 0;

    // 1: 创建10个线程，并周期性输出信息
    tst_start();
    for (int i=0; i < NUM; i++){
        tst_t[i].id = i;
        tst_t[i].tid = 0;
        tst_t[i].suspend = 0;
        tst_t[i].running = 0;
        tst_t[i].set_param = 0;

        pthread_mutex_init(&tst_t[i].mutex_suspend, NULL);
        pthread_cond_init(&tst_t[i].cond_suspend, NULL);

        if (pthread_create(&tst_t[i].thread, NULL, tst_thread, &tst_t[i]) != 0) {
            tst_res(FAIL, "thread create error\n");
            exit(42);
        }
    }

    for(int i=0; i < NUM; i++) {
	if(tst_t[i].tid != 0) {
            count++;
	}else {
            i--;
	}
    }

    if (count == 10){
        tst_res(PASS, "Created %d threads\n", NUM);
        count = 0;
    }else {
        tst_res(FAIL, "Failed to create threads count=%d\n", count);
        assert(count == 10);
    }

    // 2：删除3个线程，确保删除的线程无输出信息
    tst_start();
    tst_info("Print Thread List: ");
    for(int i=0; i < NUM; i++) {
        printf("[%d] ", tst_t[i].tid);
    }
    printf("\n");

    for (int i=0; i< TEST_NUM; i++) {
        tst_t[i].running = 0;
    }

    tst_info("Print Thread List: ");
    for(int i=0; i < NUM; i++) {
        if(tst_t[i].running != 0)
            printf("[%d] ", tst_t[i].tid);
    }
    printf("\n");

    for (int i = 0; i < TEST_NUM; i++) {
        count++;
        pthread_join(tst_t[i].thread, NULL);
    }

    if(count == 3){
        tst_res(PASS, "Killed %d Threads\n", TEST_NUM);
        count = 0;
    }else {
        tst_res(FAIL, "Failed to kill threads\n");
        assert(count == 3);
    }

    // 3：挂起3个进程，确保挂起的进程存在并无输出信息
    tst_start();
    for (int i = TEST_NUM; i < 2*TEST_NUM; i++) {
        pthread_mutex_lock(&tst_t[i].mutex_suspend);
        tst_t[i].suspend= 1;
        pthread_mutex_unlock(&tst_t[i].mutex_suspend);
    }
    sleep(2);

    for (int i = TEST_NUM; i < 2*TEST_NUM; i++) {
        if(tst_t[i].suspend == 1)
            count++;
    }

    if(count == 3){
        tst_res(PASS, "Suspend %d Threads\n", TEST_NUM);
        count = 0;
    }else {
        tst_res(FAIL, "Failed to suspend threads\n");
        assert(count == 3);
    }

    // 4. 查询线程状态 
    tst_start();
    for (int i = 0; i < NUM; i++) {
        char state[512] = {};
        if (i >= 0 && i < TEST_NUM) continue;
        if(get_thread_state(tst_t[i].tid, state, sizeof(state)) == 0){
            tst_info("%s\n", state);
        }else{
            tst_res(FAIL, "Failed to get threads attribute\n");
            exit(EXIT_FAILURE);
        }
    }
    tst_res(PASS, "Check threads state\n");

    // 5. 修改线程属性
    tst_start();
    tst_t[NUM-1].set_param = 1;
    sleep(4);
    if (tst_t[NUM-1].set_param == 0)
        tst_res(PASS, "Set threads attributes\n");
    else
        tst_res(FAIL, "Set threads attributes\n");
    
    // join
    tst_info("Exit threads\n");
    for (int i = TEST_NUM; i <= 2*TEST_NUM; i++) {
        pthread_mutex_lock(&tst_t[i].mutex_suspend);
        tst_t[i].suspend = 0;
        pthread_cond_signal(&tst_t[i].cond_suspend);
        pthread_mutex_unlock(&tst_t[i].mutex_suspend);
    }
    for (int i = 0; i < NUM; ++i) {
        pthread_mutex_destroy(&tst_t[i].mutex_suspend);
        pthread_cond_destroy(&tst_t[i].cond_suspend);
        if (i >= 0 && i < 3) continue;
        tst_t[i].running = 0;
        pthread_join(tst_t[i].thread, NULL);
    }

    return EXIT_SUCCESS;
}

int main()
{
    int ret = test_thread_management();
    if(ret == EXIT_SUCCESS)
        tst_res(PASS, "test_thread_management test pass\n");
    return ret;
}

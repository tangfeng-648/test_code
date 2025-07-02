#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <semaphore.h>
#include "test.h"

typedef struct {
    int id;
    int tid;
    pthread_t thread;
} tst_thread_t;

static sem_t g_semaphore;
static volatile int g_quit = 0;

#define TIME_1S

static inline pid_t oh_gettid() {
    return syscall(__NR_gettid);
}

static int get_thread_state(pid_t tid, char *stat, size_t buf_len) {
    char path[512];
    char line[512];
    snprintf(path, sizeof(path), "/proc/%d/status", tid);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return 0;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "State:", 6) == 0) {
            snprintf(stat, strlen(line), "%s", line);
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    return 0;
}

void *tst_thread(void *arg) {
    tst_thread_t* tst = (tst_thread_t*)arg;
    tst->tid = oh_gettid();

    while (g_quit == 0) {
        // 输出ID信息
        tst_info("TID%d[%d] is running\n", tst->id, tst->tid);

        // 申请信号量
        if (sem_wait(&g_semaphore) != 0) {
            tst_info("Failed to acquire semaphore\n");
            exit(42);
        }

        // 运算
#ifdef TIME_1S
        time_t start_time = time(NULL);
        while (time(NULL) - start_time < 1) {}
#else
        for (int i = 0; i < 10000000; i++) {}
#endif
        // 释放信号量
        if (sem_post(&g_semaphore) != 0) {
            tst_info("Failed to release semaphore\n");
            exit(42);
        }
    }
    tst_info("TID%d[%d] exiting\n", tst->id, tst->tid);
    pthread_exit(NULL);
    return NULL;
}

static int test_thread_state(void)
{
    long nr_cpus = 0;

    if (sem_init(&g_semaphore, 0, 1) != 0) {
        exit(42);
    }

    // 1. 获取CPU核心数
    tst_start();
    nr_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (nr_cpus <= 0) {
        exit(42);
    }
    tst_res(PASS, "Current processor has %d cores\n", nr_cpus);

    // 2. 创建3k的线程，每个线程输出ID信息，申请信号量，运算，释放信号量
    tst_start();
    nr_cpus = 3 * nr_cpus;
    tst_thread_t tst_t[nr_cpus];
    for (long i = 0; i < nr_cpus; i++) {
        tst_t[i].id = i;
        if (pthread_create(&tst_t[i].thread, NULL, tst_thread, &tst_t[i]) != 0) {
            tst_res(FAIL, "Failed to create running thread %ld\n", i);
            exit(42);
        }
    }

    // 3. 查询线程状态
    tst_start();
    int count = 0;
    while(count != 3) {
        for (int i = 0; i < nr_cpus; i++) {
            char state[512] = {};
            if(get_thread_state(tst_t[i].tid, state, sizeof(state)) == 0){
                tst_info("%d %s\n", tst_t[i].tid, state);
                if(strstr(state, "sleeping") && (count != 0x1 && count != 0x3)){
                    count |= 0x1;
                    tst_info("Has Blocked State\n");
                }
                if(strstr(state, "running") && (count != 0x2 && count != 0x3)){
                    count |= 0x2;
                    tst_info("Has Readly and Running State\n");
                }
            }else{
                tst_res(FAIL, "Failed to get threads attribute\n");
                exit(EXIT_FAILURE);
            }
        }
        sleep(1);
    }
    if(count == 0x3)
        tst_res(PASS, "Check threads state\n");
    else 
        tst_res(FAIL, "Check threads state\n");

    g_quit = 1;
    for (int i = 0; i < nr_cpus; i++) {
        pthread_join(tst_t[i].thread, NULL);
    }
    sem_destroy(&g_semaphore);

    return EXIT_SUCCESS;
}

int main()
{
    int ret = test_thread_state();
    if(ret == EXIT_SUCCESS)
        tst_res(PASS, "test_thread_state test pass\n");
    return ret;
}

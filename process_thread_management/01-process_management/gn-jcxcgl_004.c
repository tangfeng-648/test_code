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
        time_t start_time = time(NULL);
        while (time(NULL) - start_time < 2) {}

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

static int test_thread_state_change(void)
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

    // 2. 创建3*nr_cpus得线程，每个线程输出ID信息，申请信号量，运算，释放信号量
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

    // 3. 选择一个thread1,记录线程的状态变化过程
    tst_start();
    char state[512] = {};
    int thread_state = 0;
    int count = 0;
    while(1) {
        if(get_thread_state(tst_t[1].tid, state, sizeof(state)) == 0){
            if(strstr(state, "sleeping") && (thread_state != 0x1)){
                thread_state = 0x1;
                tst_info("Thread in Blocked State\n");
                count++;
            }
            if(strstr(state, "running") && (thread_state != 0x2)){
                thread_state = 0x2;
                tst_info("Thread in Readly or Running State\n");
                count++;
            }
            if(count == 3)
                break;
        }
    }

    tst_res(PASS, "Check one thread state\n");

    g_quit = 1;
    for (int i = 0; i < nr_cpus; i++) {
        pthread_join(tst_t[i].thread, NULL);
    }
    sem_destroy(&g_semaphore);

    return EXIT_SUCCESS;
}

int main()
{
    int ret = test_thread_state_change();
    if(ret == EXIT_SUCCESS)
        tst_res(PASS, "test_thread_state test pass\n");
    return ret;
}

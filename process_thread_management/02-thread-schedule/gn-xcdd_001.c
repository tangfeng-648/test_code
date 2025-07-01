#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdatomic.h>
#include <pthread.h>
#include <semaphore.h>
#include "test.h"

typedef struct {
    int id;
    int tid;
    pthread_t thread;
} tst_thread_t;

static sem_t g_semaphore;
static int g_quit = 0;
static atomic_long thread_count = 0;

static inline pid_t oh_gettid() {
    return syscall(__NR_gettid);
}

void *tst_thread(void *arg) {
    tst_thread_t* tst = (tst_thread_t*)arg;
    tst->tid = oh_gettid();

    atomic_fetch_add(&thread_count, 1);

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
        for (int i = 0; i < 1000000; i++) {}
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

static int test_thread_schedule(void)
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
    tst_thread_t tst_t[3*nr_cpus];
    for (long i = 0; i < 3 * nr_cpus; i++) {
        tst_t[i].id = i;
        if (pthread_create(&tst_t[i].thread, NULL, tst_thread, &tst_t[i]) != 0) {
            tst_res(FAIL, "Failed to create running thread %ld\n", i);
            exit(42);
        }
    }

    while (thread_count != 3*nr_cpus) {}
    tst_res(PASS, "All threads have TID info\n", nr_cpus);

    g_quit = 1;
    for (int i = 0; i < nr_cpus; i++) {
        pthread_join(tst_t[i].thread, NULL);
    }
    sem_destroy(&g_semaphore);

    return EXIT_SUCCESS;
}

int main()
{
    int ret = test_thread_schedule();
    if(ret == EXIT_SUCCESS)
        tst_res(PASS, "test_thread_schedule test pass\n");
    return ret;
}

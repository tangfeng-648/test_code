#include <stdlib.h>
#include <unistd.h>
#include "test.h"
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdatomic.h>
#define __USE_GNU
#include <sched.h>
#include <pthread.h>

#define FIFO
typedef struct {
    int id;
    int tid;
    int quit;
    int suspend;
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t makeorder;
} tst_thread_t;

static int g_quiet = 0;

static inline pid_t oh_gettid() {
    return syscall(__NR_gettid);
}

void *tst_thread(void *arg) {
    time_t t;
    struct tm* local;

    tst_thread_t* tst = (tst_thread_t*)arg;
    tst->tid = oh_gettid();

    while (tst->quit == 0) {
#ifdef FIFO
        pthread_mutex_lock(&tst->mutex);
        while(tst->suspend) {
            pthread_cond_wait(&tst->makeorder, &tst->mutex);
        }
        tst->suspend--;
        pthread_mutex_unlock(&tst->mutex);
#endif
        t = time(NULL);
        local = localtime(&t);
        // 输出ID信息和时间
        if(!g_quiet)
            tst_info("TID%d[%d] is running at %02d:%02d:%02d\n", tst->id, tst->tid, local->tm_hour, local->tm_min, local->tm_sec);

        // 运算
        while (time(NULL) - t < 1) {}
    }
    tst_info("TID%d[%d] exiting\n", tst->id, tst->tid);
    pthread_exit(NULL);
    return NULL;
}

static int print_sched_policy(int policy)
{
    switch (policy) {
        case SCHED_FIFO:
            tst_info("Current scheduling policy: SCHED_FIFO\n");
            break;
        case SCHED_RR:
            tst_info("Current scheduling policy: SCHED_RR\n");
            break;
        case SCHED_OTHER:
            tst_info("Current scheduling policy: SCHED_OTHER\n");
            break;
        default:
            tst_info("Unknown scheduling policy %d \n", policy);
            break;
    }
}

static int test_schedule_fifo(void)
{
    long nr_cpus = 0;

    // 1. 查询操作系统调度策略，设置调度策略为时间片轮转调度
    tst_start();
    struct sched_param param;
    int priority ;
    print_sched_policy(sched_getscheduler(0));
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    sched_setscheduler(0, SCHED_FIFO, &param);
    print_sched_policy(sched_getscheduler(0));
    tst_res(PASS, "Current SCHED policy is SCHED_FIFO\n");

    // 2. 创建k个线程，每个线程输出ID信息，申请信号量，运算，释放信号量
    tst_start();
    nr_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (nr_cpus <= 0) {
        exit(42);
    }

    tst_thread_t tst_t[nr_cpus];
    for (long i = 0; i < nr_cpus; i++) {
        tst_t[i].id = i;
        tst_t[i].quit = 0;
        tst_t[i].suspend = 1;
#ifdef FIFO
        pthread_mutex_init(&tst_t[i].mutex, NULL);
        pthread_cond_init(&tst_t[i].makeorder, NULL);
#endif
        if (pthread_create(&tst_t[i].thread, NULL, tst_thread, &tst_t[i]) != 0) {
            tst_res(FAIL, "Failed to create running thread %ld\n", i);
            exit(42);
        }
    }

    tst_res(PASS, "All threads[%d] has printed\n", nr_cpus);

    // 3. 设置k个线程在第i个核上运行
    tst_start();
    cpu_set_t cpuset;
    int count = 0;
    for (int i = 0; i < nr_cpus; i++) {
        CPU_ZERO(&cpuset);
        CPU_SET(i, &cpuset);
        pthread_setaffinity_np(tst_t[i].thread, sizeof(cpu_set_t), &cpuset);
    }

    for (int i = 0; i < nr_cpus; i++) {
        CPU_ZERO(&cpuset);
        pthread_getaffinity_np(tst_t[i].thread, sizeof(cpu_set_t), &cpuset);
        if (CPU_ISSET(i, &cpuset)) {
            tst_info("thread%d is running on cpu%d\n", i, i);
            count++;
        }
    }
    if(count == nr_cpus)
        tst_res(PASS, "All threads are mapped with CPU cores\n");
    else
        tst_res(FAIL, "Not all threads are mapped with CPU cores\n");

    // 4. 依次从1-k顺序激活线程
    tst_start();
    count = 0;
    for (int i = 0; i < nr_cpus; i++) {
        pthread_mutex_lock(&tst_t[i].mutex);
        tst_t[i].suspend = 0;
        pthread_cond_signal(&tst_t[i].makeorder);
        pthread_mutex_unlock(&tst_t[i].mutex);
        count++;
        usleep(10000);
    }
    if(count == nr_cpus)
        tst_res(PASS, "All threads are now mutex unlocked\n");
    else
        tst_res(FAIL, "Not all threads are now mutex unlocked\n");
    sleep(1);

    // 5. 依次从k-1顺序激活线程
    tst_start();
    for (int i = nr_cpus - 1; i != -1; i--) {
        pthread_mutex_lock(&tst_t[i].mutex);
        tst_t[i].suspend = 1;
        pthread_cond_signal(&tst_t[i].makeorder);
        pthread_mutex_unlock(&tst_t[i].mutex);
        count++;
        usleep(10000);
    }

    count = 0;
    for (int i = nr_cpus - 1; i != -1; i--) {
        pthread_mutex_lock(&tst_t[i].mutex);
        tst_t[i].suspend = 0;
        pthread_cond_signal(&tst_t[i].makeorder);
        pthread_mutex_unlock(&tst_t[i].mutex);
        count++;
        usleep(10000);
    }
    if(count == nr_cpus)
        tst_res(PASS, "All threads are now mutex unlocked (reverse)\n");
    else
        tst_res(FAIL, "Not all threads are now mutex unlocked (reverse)\n");
    sleep(1);

    // 6. 依次删除k个进程
    tst_start();
    count = 0;
    for (int i = 0; i < nr_cpus; i++) {
        pthread_mutex_lock(&tst_t[i].mutex);
        tst_t[i].suspend = 0;
        tst_t[i].quit = 1;
        pthread_cond_signal(&tst_t[i].makeorder);
        pthread_mutex_unlock(&tst_t[i].mutex);
        pthread_join(tst_t[i].thread, NULL);
        pthread_mutex_destroy(&tst_t[i].mutex);
        pthread_cond_destroy(&tst_t[i].makeorder);
        count++;
        usleep(10000);
    }
    if(count == nr_cpus)
        tst_res(PASS, "All threads have quit\n");
    else
        tst_res(FAIL, "Some threads cannot quit\n");

    return EXIT_SUCCESS;
}

static int test_schedule_rr(void)
{
    long nr_cpus = 0;

    // 1. 查询操作系统调度策略，设置调度策略为时间片轮转调度
    tst_start();
    struct sched_param param;
    int priority ;
    print_sched_policy(sched_getscheduler(0));
    param.sched_priority = sched_get_priority_max(SCHED_RR);
    sched_setscheduler(0, SCHED_RR, &param);
    print_sched_policy(sched_getscheduler(0));
    tst_res(PASS, "Current SCHED policy is SCHED_RR\n");

    // 2. 创建k个线程，每个线程输出ID信息，申请信号量，运算，释放信号量
    tst_start();
    nr_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (nr_cpus <= 0) {
        exit(42);
    }

    tst_thread_t tst_t[nr_cpus];
    for (long i = 0; i < nr_cpus; i++) {
        tst_t[i].id = i;
        tst_t[i].quit = 0;
        if (pthread_create(&tst_t[i].thread, NULL, tst_thread, &tst_t[i]) != 0) {
            tst_res(FAIL, "Failed to create running thread %ld\n", i);
            exit(42);
        }
    }
    tst_res(PASS, "All threads[%d] has printed\n", nr_cpus);

    // 3. 设置k个线程在第i个核上运行
    tst_start();
    cpu_set_t cpuset;
    for (int i = 0; i < nr_cpus; i++) {
        CPU_ZERO(&cpuset);
        CPU_SET(i, &cpuset);
        pthread_setaffinity_np(tst_t[i].thread, sizeof(cpu_set_t), &cpuset);
    }

    int count = 0;
    for (int i = 0; i < nr_cpus; i++) {
        CPU_ZERO(&cpuset);
        pthread_getaffinity_np(tst_t[i].thread, sizeof(cpu_set_t), &cpuset);
        if (CPU_ISSET(i, &cpuset)) {
            tst_info("thread%d is running on cpu%d\n", i, i);
            count++;
        }
    }
    if(count == nr_cpus)
        tst_res(PASS, "All threads are mapped with CPU cores\n");
    else
        tst_res(FAIL, "Not all threads are mapped with CPU cores\n");

    g_quiet = 1;
    for (int i = 0; i < nr_cpus; i++) {
        tst_t[i].quit = 1;
        pthread_join(tst_t[i].thread, NULL);
    }

    return EXIT_SUCCESS;
}

static int test_schedule()
{
    int ret = -1;
#ifdef FIFO
    ret = test_schedule_fifo();
#else  // RR
    ret = test_schedule_rr();
#endif
    return ret;
}

int main()
{
    int ret = test_schedule();
    if(ret == EXIT_SUCCESS)
        tst_res(PASS, "test_schedule test pass\n");
    return ret;
}

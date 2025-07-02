#include <sys/prctl.h>
#include <stdlib.h>
#include <ctype.h>
#include <sched.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <dirent.h>
#include <pthread.h>
#include "test.h"
#include "list.h"

static pid_t p_pid;

#define ALLOC_SIZE (1024 * 1024 * 100)

typedef struct {
    char name[256];
    pid_t pid;
    struct list_head list;
} process_list_t;

int is_uint(char input[])
{
    for (int i=0; i < strlen(input); i++)
    {
        if(!isdigit(input[i]))
        {
            return 0;
        }
    }
    return 1;
}

int read_proc(struct list_head* plist)
{
    char* sched_path;
    DIR * directory;
    struct dirent * dir;
    FILE *sched_file;

    sched_path = malloc(sizeof(char)*256);

    directory=opendir("/proc");
    if(directory==NULL)
    {
        tst_info("Cannot open /proc\n");
        exit(-42);
    }

    while((dir = readdir(directory)) != NULL)
    {
        if(!is_uint(dir->d_name))
        {
            continue;
        }
        process_list_t* p = malloc(sizeof(process_list_t));
        p->pid = atoi(dir->d_name);

        strcpy(sched_path, "/proc/");
        strcat(sched_path, dir->d_name);
        strcat(sched_path, "/sched");

        sched_file = fopen(sched_path, "r");
        if (sched_file) {
            char line[512] = {};
            char comm[256] = {};
            char *name;
            int policy;
            if (fgets(comm, sizeof(comm), sched_file)) {
                comm[strcspn(comm, "\n")] = 0;
                name = strtok(comm, " ");
            }

            while (fgets(line, sizeof(line), sched_file)) {
                if (strncmp(line, "policy", 6) == 0) {
                    if (sscanf(line, "policy\t:\t%d", &policy) == 1) {
                        if(policy == 1) {
                            strncpy(p->name, name, strlen(name));
                            list_add(&p->list, plist);
                        }
                        break;
                    }
                    break;
                }
            }
            fclose(sched_file);
        }
    }

    free(sched_path);
    return 0;
}

void print_process_list(struct list_head* plist)
{
    list_for_each_entry_rev(process_list_t, pos, plist, list) {
        printf("%d[%s] \n", pos->pid, pos->name);
    }
}

void free_plist(struct list_head* plist)
{
    list_for_each_entry_safe(process_list_t, pos, plist, list) {
        list_del(&pos->list);
        free(pos);
    }
}

void oom()
{
    struct sched_param param;
    size_t alloc_count = 0;
    FILE *oom_fd;
    void **allocs = (void **)malloc(sizeof(void *) * 1024);

    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        tst_info("Failed to set FIFO priority\n");
        exit(42);
    }

    if (setpriority(PRIO_PROCESS, 0, -20) != 0) {
        tst_info("Failed to set nice priority\n");
        exit(42);
    }

    oom_fd = fopen("/proc/self/oom_score_adj", "w");
    if (oom_fd) {
        const char* score = "1000";
        if (fwrite(score, sizeof(score), 1, oom_fd) != 1) {
            tst_info("Failed to set pid[%d] oom_score_adj\n", getpid());
            exit(42);
        }
        fclose(oom_fd);
    }

    while (1) {
        void *mem = malloc(ALLOC_SIZE);
        if (!mem) {
            break;
        }

        allocs[alloc_count] = mem;
        alloc_count++;

        memset(mem, 0x55, ALLOC_SIZE);
        tst_info("Allocated %d MB\n", ALLOC_SIZE / 1024 / 1024);

        usleep(10000);
    }

    for (int i = 0; i < alloc_count; i++) {
        free(allocs[i]);
    }
    free(allocs);
    return;
}

void oom_process()
{
    int pid = fork();
    int status = 0;

    if (pid < 0) {
        tst_res(FAIL, "fork error\n");
        exit(42);
    }else if (pid == 0){
        prctl(PR_SET_NAME, "loader", 0, 0, 0);
        oom();
        exit(0);
    }

    if(pid > 0) {
        waitpid(pid, &status, 0);
    }

    return;
}

static void print_time()
{
    time_t t;
    struct tm* local;
    struct sched_param param;
    FILE *fd;
    sigset_t sigset;

    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        tst_info("Failed to set FIFO priority\n");
        exit(42);
    }

    sigfillset(&sigset);
    if (sigprocmask(SIG_BLOCK, &sigset, NULL) != 0) {
        exit(42);
    }

    fd = fopen("/proc/self/oom_score_adj", "w");
    if (fd) {
        const char* score = "-1000";
        if (fwrite(score, sizeof(score), 1, fd) != 1) {
            tst_info("Failed to set pid[%d] oom_score_adj\n", getpid());
            exit(42);
        }
        fclose(fd);
    }

    while(1) {
        t = time(NULL);
        local = localtime(&t);
        // 输出ID信息和时间
        tst_info("PID[%d] is running at %02d:%02d:%02d\n", getpid(), local->tm_hour, local->tm_min, local->tm_sec);
        while (time(NULL) - t < 1) {}
    }
    return ;
}

static void log_process()
{
    int pid = fork();

    if (pid < 0) {
        tst_res(FAIL, "fork error\n");
        exit(42);
    }else if (pid == 0){
        prctl(PR_SET_NAME, "P", 0, 0, 0);
        print_time();
        exit(0);
    }

    p_pid = pid;

    return;
}

static void kill_process()
{
    int pid = fork();

    if (pid < 0) {
        tst_res(FAIL, "fork error\n");
        exit(42);
    }else if (pid == 0){
        prctl(PR_SET_NAME, "P1", 0, 0, 0);
        if (kill(p_pid, SIGTERM) != 0) {
            exit(42);
        }
        exit(0);
    }

    return;
}

static int check_process_exist(pid_t pid) {
    char exe[256];
    snprintf(exe, sizeof(exe), "/proc/%d/exe", pid);
    if (access(exe, R_OK) == 0) {
        return 1;
    }else {
        return 0;
    }
}


static int test_process_prevent_processes() {
    struct list_head* core_head = malloc(sizeof(struct list_head));
    list_inithead(core_head);

    // 1. 创建一个进程P，周期输出进程ID和当前时间
    // 2. 设置P为关键进程(FIFO)
    tst_start();
    log_process();
    tst_res(PASS, "Created processe P success\n");

    // 3. 获取关键进程(FIFO)清单
    read_proc(core_head);
    print_process_list(core_head);
    tst_res(PASS, "Core processe list length=%d\n", list_length(core_head));

    // 4. 创建一个进程P1，向P进程发送KILL(TERM)信号
    tst_start();
    kill_process();
    if(check_process_exist(p_pid)) {
        tst_res(PASS, "Process P cannot be killed by SIGTERM\n");
    }else {
        tst_res(FAIL, "Process P has been killed by SIGTERM\n");
    }

    // 5. 启动一个loader进程，逐渐申请内存，直到内存不足
    tst_start();
    oom_process();
    tst_res(PASS, "Process P cannot be killed by OOM Killer\n");

    kill(p_pid, SIGKILL);
    free_plist(core_head);
    free(core_head);
    return 0;
}

int main()
{
    int ret = test_process_prevent_processes();
    if(ret == EXIT_SUCCESS)
        tst_res(PASS, "test_process_prevent_processes test pass\n");
    return ret;
}

#include <sys/prctl.h>
#include <stdlib.h>
#include <ctype.h>
#include <sched.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <dirent.h>
#include "test.h"
#include "list.h"

#define ALLOC_SIZE (1024 * 1024 * 100)

typedef struct {
    char name[NAME_MAX+1];
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
    char* comm_path;
    DIR * directory;
    struct dirent * dir;
    FILE *comm_file;

    comm_path = malloc(sizeof(char)*NAME_MAX);

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

        strcpy(comm_path, "/proc/");
        strcat(comm_path, dir->d_name);
        strcat(comm_path, "/comm");

        comm_file = fopen(comm_path, "r");
        if (comm_file) {
            char comm[256];
            if (fgets(comm, sizeof(comm), comm_file)) {
                comm[strcspn(comm, "\n")] = 0;
            }
            strncpy(p->name, comm, strlen(comm_path));
            fclose(comm_file);
        }

        list_add(&p->list, plist);
    }

    free(comm_path);
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
    FILE *oom_fd;
    size_t alloc_count = 0;
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
    exit(0);
    return;
}

void oom_process(struct list_head* plist1)
{
    int pid = fork();
    int status = 0;

    if (pid < 0) {
        tst_res(FAIL, "fork error\n");
        exit(42);
    }else if (pid == 0){
        prctl(PR_SET_NAME, "loader", 0, 0, 0);
        oom();
    }

    if(pid > 0) {
        read_proc(plist1);
        if (waitpid(pid, &status, 0) > 0) {
            if (WIFSIGNALED(status))
                tst_info("PID[%d] killed by OOM Killer[%d]\n", pid, WTERMSIG(status));
        }
    }

    return;
}

int test_oom_killer() {
    // 1. 获取进程列表plist1
    tst_start();
    struct list_head* plist1_head = malloc(sizeof(struct list_head));
    list_inithead(plist1_head);
    read_proc(plist1_head);
    print_process_list(plist1_head);
    free_plist(plist1_head);
    tst_res(PASS, "Current process list lenth=%d\n", list_length(plist1_head));

    // 2. 启动一个loader进程，逐渐申请内存，直到内存不足
    tst_start();
    oom_process(plist1_head);

    // 3. 周期性检查，获取plist2
    tst_start();
    int plist1_len = list_length(plist1_head);
    int plist2_len = 0;
    int pass = false;
    struct list_head* plist2_head = malloc(sizeof(struct list_head));

    list_inithead(plist2_head);
    while(!pass) {
        read_proc(plist2_head);
        plist2_len = list_length(plist2_head);
        if(plist1_len > plist2_len) {
            pass = true;
        }
        free_plist(plist2_head);
        sleep(1);
    }
    tst_res(PASS, "Plist1 > Plist2\n");

    free_plist(plist1_head);
    free(plist1_head);
    free(plist2_head);
    return 0;
}

int main()
{
    int ret = test_oom_killer();
    if(ret == EXIT_SUCCESS)
        tst_res(PASS, "test_oom_killer test pass\n");
    return ret;
}

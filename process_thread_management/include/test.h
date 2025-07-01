#ifndef _TST_RES_H_
#define _TST_RES_H_

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define PASS 0 
#define FAIL 1

#define tst_res(ttype, arg_fmt, ...) \
        ({                                                                      \
                test_result(__FILE__, __func__, (ttype), (arg_fmt), ##__VA_ARGS__);\
        })

#define tst_info(arg_fmt, ...) \
        ({                                                                      \
                test_info(__FILE__, __func__, (arg_fmt), ##__VA_ARGS__);\
        })

#define tst_start() printf("----------------------------------------\n");

static inline void test_result(const char *file, const char* func, int ttype, char *fmt, ...)
{
	const char* file_name = strrchr(file, '/');
    printf("%s: %s: [%s] ", file_name ? file_name + 1 : file, func, ttype?"FAIL":"PASS");

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

static inline void test_info(const char *file, const char* func, char *fmt, ...)
{
	const char* file_name = strrchr(file, '/');
    printf("%s: %s: ", file_name ? file_name + 1 : file, func);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}


#endif

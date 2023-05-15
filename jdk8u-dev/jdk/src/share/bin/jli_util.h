/*
 * Copyright (c) 2005, 2014, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#ifndef _JLI_UTIL_H
#define _JLI_UTIL_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <jni.h>
#define JLDEBUG_ENV_ENTRY "_JAVA_LAUNCHER_DEBUG"

void *JLI_MemAlloc(size_t size);
void *JLI_MemRealloc(void *ptr, size_t size);
char *JLI_StringDup(const char *s1);
void  JLI_MemFree(void *ptr);
int   JLI_StrCCmp(const char *s1, const char* s2);

typedef struct {
    char *arg;
    jboolean has_wildcard;
} StdArg;

StdArg *JLI_GetStdArgs();
int     JLI_GetStdArgc();

#define JLI_StrLen(p1)          strlen((p1))
#define JLI_StrChr(p1, p2)      strchr((p1), (p2))
#define JLI_StrRChr(p1, p2)     strrchr((p1), (p2))
#define JLI_StrCmp(p1, p2)      strcmp((p1), (p2))
#define JLI_StrNCmp(p1, p2, p3) strncmp((p1), (p2), (p3))
#define JLI_StrCat(p1, p2)      strcat((p1), (p2))
#define JLI_StrCpy(p1, p2)      strcpy((p1), (p2))
#define JLI_StrNCpy(p1, p2, p3) strncpy((p1), (p2), (p3))
#define JLI_StrStr(p1, p2)      strstr((p1), (p2))
#define JLI_StrSpn(p1, p2)      strspn((p1), (p2))
#define JLI_StrCSpn(p1, p2)     strcspn((p1), (p2))
#define JLI_StrPBrk(p1, p2)     strpbrk((p1), (p2))

/* On Windows lseek() is in io.h rather than the location dictated by POSIX. */
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <process.h>
#define JLI_StrCaseCmp(p1, p2)          stricmp((p1), (p2))
#define JLI_StrNCaseCmp(p1, p2, p3)     strnicmp((p1), (p2), (p3))
int  JLI_Snprintf(char *buffer, size_t size, const char *format, ...);
void JLI_CmdToArgs(char *cmdline);
#define JLI_Lseek                       _lseeki64
#define JLI_PutEnv                      _putenv
#define JLI_GetPid                      _getpid
#else  /* NIXES */
#include <unistd.h>
#include <strings.h>
#define JLI_StrCaseCmp(p1, p2)          strcasecmp((p1), (p2))
#define JLI_StrNCaseCmp(p1, p2, p3)     strncasecmp((p1), (p2), (p3))
#define JLI_Snprintf                    snprintf
#define JLI_PutEnv                      putenv
#define JLI_GetPid                      getpid
#ifdef __solaris__
#define JLI_Lseek                       llseek
#endif
#ifdef __linux__
#define _LARGFILE64_SOURCE
#define JLI_Lseek                       lseek64
#endif
#ifdef MACOSX
#define JLI_Lseek                       lseek
#endif
#ifdef _AIX
#define JLI_Lseek                       lseek
#endif
#endif /* _WIN32 */

/*
 * Make launcher spit debug output.
 */
void     JLI_TraceLauncher(const char* fmt, ...);
void     JLI_SetTraceLauncher();
jboolean JLI_IsTraceLauncher();

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>

/* SLog version information */
#define SLOG_VERSION_MAJOR      1
#define SLOG_VERSION_MINOR      8
#define SLOG_BUILD_NUM          31

/* Supported colors */
#define SLOG_COLOR_NORMAL       "\x1B[0m"
#define SLOG_COLOR_RED          "\x1B[31m"
#define SLOG_COLOR_GREEN        "\x1B[32m"
#define SLOG_COLOR_YELLOW       "\x1B[33m"
#define SLOG_COLOR_BLUE         "\x1B[34m"
#define SLOG_COLOR_MAGENTA      "\x1B[35m"
#define SLOG_COLOR_CYAN         "\x1B[36m"
#define SLOG_COLOR_WHITE        "\x1B[37m"
#define SLOG_COLOR_RESET        "\033[0m"

/**
* 该声明主要功能定义输出日志的头包含打印日志所在文件、所在函数、所在行数。同时为了便于查看加入终端实时输出时日志的颜色区分，以及从行首覆盖输出的设置。
*
* #include <string.h> //strrchr()函数所需头文件
* 函数原型：char *strrchr(const char *s, int c);
* 函数功能：The strrchr() function returns a pointer to the last occurrence of the character 【c】 in the string 【s】.
函数返回一个指向最后一次出现在字符串s中的字符c的位置指针，如果c不在s中，返回NULL。
PS：linux中提供了相应的函数：basename(s)，用来获取不带路径的文件名。
*/
#define filename(x) strrchr(x, '/') ? strrchr(x, '/') + 1 : x

/* Trace source location helpers */
#define SLOG_TRACE_LVL1(LINE) #LINE
#define SLOG_TRACE_LVL2(LINE) SLOG_TRACE_LVL1(LINE)
#define FILE_LINE_FUNCTION_PLACEHOLDER "[%s:%s]-%s=>"

/* SLog limits (To be safe while avoiding dynamic allocations) */
#define SLOG_MESSAGE_MAX        8196
#define SLOG_VERSION_MAX        128
#define SLOG_PATH_MAX           2048
#define SLOG_INFO_MAX           512
#define SLOG_NAME_MAX           256
#define SLOG_DATE_MAX           64
#define SLOG_TAG_MAX            128
#define SLOG_COLOR_MAX          16

#define SLOG_FLAGS_CHECK(c, f) (((c) & (f)) == (f))
#define SLOG_FLAGS_ALL          255

#define SLOG_NAME_DEFAULT       "slog"
#define SLOG_NEWLINE            "\n"
#define SLOG_INDENT             "       "
#define SLOG_SPACE              " "
#define SLOG_EMPTY              ""
#define SLOG_NUL                '\0'

typedef struct SLogDate {
    uint16_t nYear;
    uint8_t nMonth;
    uint8_t nDay;
    uint8_t nHour;
    uint8_t nMin;
    uint8_t nSec;
    uint16_t nUsec;
} slog_date_t;

uint16_t slog_get_usec();

void slog_get_date(slog_date_t *pDate);

/* Log level flags */
typedef enum {
    SLOG_NOTAG = (1 << 0),
    SLOG_NOTE = (1 << 1),
    SLOG_INFO = (1 << 2),
    SLOG_WARN = (1 << 3),
    SLOG_DEBUG = (1 << 4),
    SLOG_TRACE = (1 << 5),
    SLOG_ERROR = (1 << 6),
    SLOG_FATAL = (1 << 7)
} slog_flag_t;

typedef int(*slog_cb_t)(const char *pLog, size_t nLength, slog_flag_t eFlag, void *pCtx);

/* Output coloring control flags */
typedef enum {
    SLOG_COLORING_DISABLE = 0,
    SLOG_COLORING_TAG,
    SLOG_COLORING_FULL
} slog_coloring_t;

typedef enum {
    SLOG_TIME_DISABLE = 0,
    SLOG_TIME_ONLY,
    SLOG_DATE_FULL
} slog_date_ctrl_t;

#define slog(eFlag, format, ...) slog_display(eFlag, FILE_LINE_FUNCTION_PLACEHOLDER format, __FILE__ , SLOG_TRACE_LVL2(__LINE__), __FUNCTION__, ##__VA_ARGS__)
#define slog_note(format, ...) slog_display(SLOG_NOTE, FILE_LINE_FUNCTION_PLACEHOLDER format, __FILE__ , SLOG_TRACE_LVL2(__LINE__), __FUNCTION__, ##__VA_ARGS__)
#define slog_info(format, ...) slog_display(SLOG_INFO, FILE_LINE_FUNCTION_PLACEHOLDER format, __FILE__ , SLOG_TRACE_LVL2(__LINE__), __FUNCTION__, ##__VA_ARGS__)
#define slog_warn(format, ...) slog_display(SLOG_WARN, FILE_LINE_FUNCTION_PLACEHOLDER format, __FILE__ , SLOG_TRACE_LVL2(__LINE__), __FUNCTION__, ##__VA_ARGS__)
#define slog_debug(format, ...) slog_display(SLOG_DEBUG, FILE_LINE_FUNCTION_PLACEHOLDER format, __FILE__ , SLOG_TRACE_LVL2(__LINE__), __FUNCTION__, ##__VA_ARGS__)
#define slog_error(format, ...) slog_display(SLOG_ERROR, FILE_LINE_FUNCTION_PLACEHOLDER format, __FILE__ , SLOG_TRACE_LVL2(__LINE__), __FUNCTION__, ##__VA_ARGS__)
#define slog_trace(format, ...) slog_display(SLOG_TRACE, FILE_LINE_FUNCTION_PLACEHOLDER format, __FILE__ , SLOG_TRACE_LVL2(__LINE__), __FUNCTION__, ##__VA_ARGS__)
#define slog_fatal(format, ...) slog_display(SLOG_FATAL, FILE_LINE_FUNCTION_PLACEHOLDER format, __FILE__ , SLOG_TRACE_LVL2(__LINE__), __FUNCTION__, ##__VA_ARGS__)

/* slog short location */
#define slog_s(eFlag, format, ...) slog_display(eFlag, FILE_LINE_FUNCTION_PLACEHOLDER format, filename(__FILE__) , SLOG_TRACE_LVL2(__LINE__), __FUNCTION__, ##__VA_ARGS__)
#define slog_note_s(format, ...) slog_display(SLOG_NOTE, FILE_LINE_FUNCTION_PLACEHOLDER format, filename(__FILE__) , SLOG_TRACE_LVL2(__LINE__), __FUNCTION__, ##__VA_ARGS__)
#define slog_info_s(format, ...) slog_display(SLOG_INFO, FILE_LINE_FUNCTION_PLACEHOLDER format, filename(__FILE__) , SLOG_TRACE_LVL2(__LINE__), __FUNCTION__, ##__VA_ARGS__)
#define slog_warn_s(format, ...) slog_display(SLOG_WARN, FILE_LINE_FUNCTION_PLACEHOLDER format, filename(__FILE__) , SLOG_TRACE_LVL2(__LINE__), __FUNCTION__, ##__VA_ARGS__)
#define slog_debug_s(format, ...) slog_display(SLOG_DEBUG, FILE_LINE_FUNCTION_PLACEHOLDER format, filename(__FILE__) , SLOG_TRACE_LVL2(__LINE__), __FUNCTION__, ##__VA_ARGS__)
#define slog_error_s(format, ...) slog_display(SLOG_ERROR, FILE_LINE_FUNCTION_PLACEHOLDER format, filename(__FILE__) , SLOG_TRACE_LVL2(__LINE__), __FUNCTION__, ##__VA_ARGS__)
#define slog_trace_s(format, ...) slog_display(SLOG_TRACE, FILE_LINE_FUNCTION_PLACEHOLDER format, filename(__FILE__) , SLOG_TRACE_LVL2(__LINE__), __FUNCTION__, ##__VA_ARGS__)
#define slog_fatal_s(format, ...) slog_display(SLOG_FATAL, FILE_LINE_FUNCTION_PLACEHOLDER format, filename(__FILE__) , SLOG_TRACE_LVL2(__LINE__), __FUNCTION__, ##__VA_ARGS__)


typedef struct SLogConfig {
    slog_date_ctrl_t eDateControl;      // Display output with date format
    slog_coloring_t eColorFormat;       // Output color format control
    slog_cb_t logCallback;              // Log callback to collect logs
    void* pCallbackCtx;                 // Data pointer passed to log callback

    uint8_t nTraceTid;                // Trace thread ID and display in output
    uint8_t nToScreen;                // Enable screen logging
    uint8_t nNewLine;                 // Enable new line ending
    uint8_t nUseHeap;                 // Use dynamic allocation
    uint8_t nToFile;                  // Enable file logging
    uint8_t nIndent;                  // Enable indentations
    uint8_t nFlush;                   // Flush stdout after screen log
    uint16_t nFlags;                  // Allowed log level flags

    char sSeparator[SLOG_NAME_MAX];     // Separator between info and log
    char sFileName[SLOG_NAME_MAX];      // Output file name for logs
    char sFilePath[SLOG_PATH_MAX];      // Output file path for logs
} slog_config_t;

size_t slog_version(char *pDest, size_t nSize, uint8_t nMin);
slog_config_t * slog_config_get();
void slog_config_set(slog_config_t * pCfg);

void slog_separator_set(const char *pFormat, ...);
void slog_callback_set(slog_cb_t callback, void *pContext);
void slog_new_line(uint8_t nEnable);
void slog_indent(uint8_t nEnable);

void slog_enable(slog_flag_t eFlag);
void slog_disable(slog_flag_t eFlag);

void slog_init(const char* pName, uint16_t nFlags, uint8_t nTdSafe);
/*
    https://zhuanlan.zhihu.com/p/55768933
    __attribute__ format(printf, 1,2)的具体含义:
    这句主要作用是提示编译器，对这个函数的调用需要像printf一样，用对应的format字符串来check可变参数的数据类型。

    例如:
    extern int my_printf (void *my_object, const char *my_format, ...)
        __attribute__ ((format (printf, 2, 3)));

    format (printf, 2, 3)告诉编译器，my_format相当于printf的format，而可变参数是从my_printf的第3个参数开始。

    这样编译器就会在编译时用和printf一样的check法则来确认可变参数是否正确了。
 */

void __attribute__((format(printf, 2, 3))) slog_display(slog_flag_t eFlag, const char *pFormat, ...);
void slog_destroy(); // Required only if the slog_init() called with nTdSafe > 0

#ifdef __cplusplus
}
#endif


#endif  /* _JLI_UTIL_H */

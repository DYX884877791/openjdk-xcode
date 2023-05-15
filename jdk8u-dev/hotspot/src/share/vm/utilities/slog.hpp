/*
 * The MIT License (MIT)
 *
 *  Copyleft (C) 2015-2020  Sun Dro (f4tb0y@protonmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE
 */

#ifndef __SLOG_H__
#define __SLOG_H__

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

#endif /* __SLOG_H__ */
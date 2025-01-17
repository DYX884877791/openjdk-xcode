/*
 * The MIT License (MIT)
 *
 *  Copyleft (C) 2015-2023  Sun Dro (s.kalatoz@gmail.com)
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include "slog.hpp"

#include <pthread.h>

#ifdef __linux__
#include <syscall.h>
#endif

#include <sys/time.h>

#ifdef WIN32
#include <windows.h>
#endif

#ifndef PTHREAD_MUTEX_RECURSIVE
#define PTHREAD_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE_NP
#endif

int
str_ends_with(const char *s1, const char *s2) {
    unsigned int len1 = strlen(s1);
    unsigned int len2 = strlen(s2);
    if (len1 < len2) {
        return -1;
    }
    for (unsigned int i = 1; i <= len2; i++) {
        if (s1[len1 - i] != s2[len2 - i]) {
            return -1;
        }
    }
    return 0;
}

typedef struct slog {
    unsigned int nTdSafe: 1;
    pthread_mutex_t mutex;
    slog_config_t *config;
} slog_t;

typedef struct XLogCtx {
    const char *pFormat;
    slog_flag_t eFlag;
    slog_date_t date;
} slog_context_t;

static slog_t *g_slog;

static void slog_sync_init(slog_t *pSlog) {
    if (!pSlog->nTdSafe) {
        return;
    }
    pthread_mutexattr_t mutexAttr;

    if (pthread_mutexattr_init(&mutexAttr) ||
        pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_RECURSIVE) ||
        pthread_mutex_init(&pSlog->mutex, &mutexAttr) ||
        pthread_mutexattr_destroy(&mutexAttr)) {
        printf("<%s:%d> %s: [ERROR] Can not initialize mutex: %d\n",
               __FILE__, __LINE__, __func__, errno);

        exit(EXIT_FAILURE);
    }
}

static void slog_lock(slog_t *pSlog) {
    if (pSlog->nTdSafe && pthread_mutex_lock(&pSlog->mutex)) {
        printf("<%s:%d> %s: [ERROR] Can not lock mutex: %d\n", __FILE__, __LINE__, __func__, errno);

        exit(EXIT_FAILURE);
    }
}

static void slog_unlock(slog_t *pSlog) {
    if (pSlog->nTdSafe && pthread_mutex_unlock(&pSlog->mutex)) {
        printf("<%s:%d> %s: [ERROR] Can not unlock mutex: %d\n", __FILE__, __LINE__, __func__, errno);
        exit(EXIT_FAILURE);
    }
}

static const char *slog_get_indent(slog_flag_t eFlag) {
    slog_config_t *pCfg = g_slog->config;
    if (!pCfg->nIndent) {
        return SLOG_EMPTY;
    }

    switch (eFlag) {
        case SLOG_ALL:
        case SLOG_TRACE:
        case SLOG_DEBUG:
        case SLOG_INFO:
        case SLOG_WARN:
            return SLOG_SPACE;
        case SLOG_ERROR:
        case SLOG_FATAL:
        default:
            break;
    }

    return SLOG_EMPTY;
}

static const char *slog_get_tag(slog_flag_t eFlag) {
    switch (eFlag) {
        case SLOG_ALL:
            return "ALL";
        case SLOG_TRACE:
            return "TRACE";
        case SLOG_DEBUG:
            return "DEBUG";
        case SLOG_INFO:
            return "INFO";
        case SLOG_WARN:
            return "WARN";
        case SLOG_ERROR:
            return "ERROR";
        case SLOG_FATAL:
            return "FATAL";
        case SLOG_NONE:
            return "NONE";
        default:
            break;
    }

    return NULL;
}


const char *slog_get_all_levels() {
    return "ALL,TRACE,DEBUG,INFO,WARN,ERROR,FATAL,NONE";
}

static const char *slog_get_color(slog_flag_t eFlag) {
    switch (eFlag) {
        case SLOG_TRACE:
            return SLOG_COLOR_CYAN;
        case SLOG_DEBUG:
            return SLOG_COLOR_BLUE;
        case SLOG_INFO:
            return SLOG_COLOR_GREEN;
        case SLOG_WARN:
            return SLOG_COLOR_YELLOW;
        case SLOG_ERROR:
            return SLOG_COLOR_RED;
        case SLOG_FATAL:
            return SLOG_COLOR_MAGENTA;
        default:
            break;
    }

    return SLOG_EMPTY;
}

uint16_t slog_get_usec() {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0) return 0;
    return (uint16_t) (tv.tv_usec / 1000);
}

void slog_get_date(slog_date_t *pDate) {
    struct tm timeinfo;
    time_t rawtime = time(NULL);
#ifdef WIN32
    localtime_s(&timeinfo, &rawtime);
#else
    localtime_r(&rawtime, &timeinfo);
#endif

    pDate->nYear = timeinfo.tm_year + 1900;
    pDate->nMonth = timeinfo.tm_mon + 1;
    pDate->nDay = timeinfo.tm_mday;
    pDate->nHour = timeinfo.tm_hour;
    pDate->nMin = timeinfo.tm_min;
    pDate->nSec = timeinfo.tm_sec;
    pDate->nUsec = slog_get_usec();
}

/*
 * https://blog.csdn.net/weixin_42604188/article/details/112614807
 * https://easeapi.com/blog/158-thread-id.html
 * http://javagoo.com/linux/gettid.html
 * http://javagoo.com/linux/gettid2.html
 * https://cloud.tencent.com/developer/article/2064467
 */
static size_t slog_get_tid() {
    return (size_t) pthread_self();
}

/**
 * 获取内核级线程ID
 * @return
 */
static size_t slog_get_kernel_tid() {
    /**
     * 也可以使用SYS_gettid宏
     * SYS_gettid：在<bits/syscall.h>中有以下内容：#define SYS_gettid __NR_gettid
     * 用户线程与内核线程之间的区别：见https://easeapi.com/blog/158-thread-id.html。
     * pthread_self(): 用户态线程id
     * __NR_gettid: 内核线程id
     */
#if defined(LINUX)
    return (size_t)syscall(__NR_gettid);
#elif defined(__APPLE__)
    return (size_t) pthread_mach_thread_np(pthread_self());
#endif
}

static size_t slog_get_pid() {
    return (size_t) getpid();
}

static void slog_create_tag(char *pOut, size_t nSize, slog_flag_t eFlag, const char *pColor) {
    slog_config_t *pCfg = g_slog->config;
    pOut[0] = SLOG_NUL;

    const char *pIndent = slog_get_indent(eFlag);
    const char *pTag = slog_get_tag(eFlag);

    if (pTag == NULL) {
        snprintf(pOut, nSize, "%s", pIndent);
        return;
    }

    if (pCfg->eColorFormat != SLOG_COLORING_TAG) {
        snprintf(pOut, nSize, "<%s>%s", pTag, pIndent);
    } else {
        snprintf(pOut, nSize, "%s<%s>%s%s", pColor, pTag, SLOG_COLOR_RESET, pIndent);
    }
}

static void slog_create_pid(char *pOut, int nSize, uint8_t nTracePid) {
    if (!nTracePid) {
        pOut[0] = SLOG_NUL;
    } else {
        snprintf(pOut, nSize, "[process-id:%lu] ", slog_get_pid());
    }
}

static void slog_create_tid(char *pOut, int nSize, uint8_t nTraceTid) {
    if (!nTraceTid) {
        pOut[0] = SLOG_NUL;
    } else {
        snprintf(pOut, nSize, "[user-thread-id:0x%016lx,kernel-thread-id:0x%lx] ", slog_get_tid(), slog_get_kernel_tid());
    }
}


static void slog_display_message(const slog_context_t *pCtx, const char *pInfo, int nInfoLen, const char *pInput) {
    slog_config_t *pCfg = g_slog->config;
    int nCbVal = 1;

    uint8_t nFullColor = pCfg->eColorFormat == SLOG_COLORING_FULL ? 1 : 0;
    const char *pSeparator = nInfoLen > 0 ? pCfg->sSeparator : SLOG_EMPTY;
    const char *pNewLine = pCfg->nNewLine ? SLOG_NEWLINE : SLOG_EMPTY;
    const char *pMessage = pInput != NULL ? pInput : SLOG_EMPTY;
    const char *pReset = nFullColor ? SLOG_COLOR_RESET : SLOG_EMPTY;

    if (pCfg->logCallback != NULL) {
        size_t nLength = 0;
        char *pLog = NULL;

        nLength += asprintf(&pLog, "%s%s%s%s%s", pInfo, pSeparator, pMessage, pReset, pNewLine);
        if (pLog != NULL) {
            nCbVal = pCfg->logCallback(pLog, nLength, pCtx->eFlag, pCfg->pCallbackCtx);
            free(pLog);
        }
    }

    if (pCfg->nToScreen) {
        printf("%s%s%s%s%s", pInfo, pSeparator, pMessage, pReset, pNewLine);
        if (pCfg->nFlush) {
            fflush(stdout);
        }
    }

    if (!pCfg->nToFile) {
        return;
    }
    const slog_date_t *pDate = &pCtx->date;

    char sFilePath[SLOG_PATH_MAX + SLOG_NAME_MAX + SLOG_DATE_MAX];
    snprintf(sFilePath, sizeof(sFilePath), "%s/%s-%04d-%02d-%02d.log", pCfg->sFilePath, pCfg->sFileName, pDate->nYear,
             pDate->nMonth, pDate->nDay);

    FILE *pFile = fopen(sFilePath, "a");
    if (pFile == NULL) {
        return;
    }

    fprintf(pFile, "%s%s%s%s%s", pInfo, pSeparator, pMessage, pReset, pNewLine);
    fclose(pFile);
}


static int slog_create_info(const slog_context_t *pCtx, char *pOut, size_t nSize) {
    slog_config_t *pCfg = g_slog->config;
    const slog_date_t *pDate = &pCtx->date;

    char sDate[SLOG_DATE_MAX + SLOG_NAME_MAX];
    sDate[0] = SLOG_NUL;

    if (pCfg->eDateControl == SLOG_TIME_ONLY) {
        snprintf(sDate, sizeof(sDate), "%02d:%02d:%02d.%03d ",
                 pDate->nHour, pDate->nMin, pDate->nSec, pDate->nUsec);
    } else if (pCfg->eDateControl == SLOG_DATE_FULL) {
        snprintf(sDate, sizeof(sDate), "%04d-%02d-%02d %02d:%02d:%02d,%03d ",
                 pDate->nYear, pDate->nMonth, pDate->nDay, pDate->nHour,
                 pDate->nMin, pDate->nSec, pDate->nUsec);
    }

    char sTid[SLOG_TAG_MAX], sPid[SLOG_TAG_MAX], sTag[SLOG_TAG_MAX];
    uint8_t nFullColor = pCfg->eColorFormat == SLOG_COLORING_FULL ? 1 : 0;

    const char *pColorCode = slog_get_color(pCtx->eFlag);
    const char *pColor = nFullColor ? pColorCode : SLOG_EMPTY;

    slog_create_tid(sTid, sizeof(sTid), pCfg->nTraceTid);
    slog_create_pid(sPid, sizeof(sPid), pCfg->nTraceTid);
    slog_create_tag(sTag, sizeof(sTag), pCtx->eFlag, pColorCode);
    // 先后顺序：时间 -- 进程 -- 线程 -- tag
    return snprintf(pOut, nSize, "%s%s%s%s%s", pColor, sDate, sPid, sTid, sTag);
}

static void slog_display_heap(const slog_context_t *pCtx, va_list args) {
    size_t nBytes = 0;
    char *pMessage = NULL;
    char sLogInfo[SLOG_INFO_MAX];

    nBytes += vasprintf(&pMessage, pCtx->pFormat, args);
    va_end(args);

    if (pMessage == NULL) {
        printf("<%s:%d> %s<error>%s %s: Can not allocate memory for input: errno(%d)\n", __FILE__, __LINE__,
               SLOG_COLOR_RED, SLOG_COLOR_RESET, __func__, errno);

        return;
    }

    int nLength = slog_create_info(pCtx, sLogInfo, sizeof(sLogInfo));
    slog_display_message(pCtx, sLogInfo, nLength, pMessage);
    if (pMessage != NULL) {
        free(pMessage);
    }
}

static void slog_display_stack(const slog_context_t *pCtx, va_list args) {
    char sMessage[SLOG_MESSAGE_MAX];
    char sLogInfo[SLOG_INFO_MAX];

    vsnprintf(sMessage, sizeof(sMessage), pCtx->pFormat, args);
    int nLength = slog_create_info(pCtx, sLogInfo, sizeof(sLogInfo));
    slog_display_message(pCtx, sLogInfo, nLength, sMessage);
}

void slog_display(slog_flag_t eFlag, const char *pFormat, ...) {
    if (g_slog == NULL) {
        fprintf(stderr, "Slog do not initialized correctly, please call slog_init first!\n");
        return;
    }
    if (pFormat == NULL) {
        return;
    }

    if (g_slog->config->nFlags == SLOG_NONE) {
        return;
    }

    if ((SLOG_FLAGS_CHECK(g_slog->config->nFlags, eFlag)) && (g_slog->config->nToScreen || g_slog->config->nToFile)) {
        slog_lock(g_slog);

        slog_context_t ctx;
        slog_get_date(&ctx.date);

        ctx.eFlag = eFlag;
        ctx.pFormat = pFormat;

        void (*slog_display_args)(const slog_context_t *pCtx, va_list args);
        slog_display_args = g_slog->config->nUseHeap ? slog_display_heap : slog_display_stack;

        va_list args;
        va_start(args, pFormat);
        slog_display_args(&ctx, args);
        va_end(args);

        slog_unlock(g_slog);
    }
}

size_t slog_version(char *pDest, size_t nSize, uint8_t nMin) {
    size_t nLength = 0;

    /* Version short */
    if (nMin)
        nLength = snprintf(pDest, nSize, "%d.%d.%d",
                           SLOG_VERSION_MAJOR, SLOG_VERSION_MINOR, SLOG_BUILD_NUM);

        /* Version long */
    else
        nLength = snprintf(pDest, nSize, "%d.%d build %d (%s)",
                           SLOG_VERSION_MAJOR, SLOG_VERSION_MINOR, SLOG_BUILD_NUM, __DATE__);

    return nLength;
}

slog_config_t *slog_config_get() {
    if (g_slog == NULL) {
        fprintf(stderr, "Slog do not initialized correctly, please call slog_init first!\n");
        return NULL;
    }
    slog_lock(g_slog);
    slog_config_t *pCfg = g_slog->config;
    slog_unlock(g_slog);
    return pCfg;
}

void slog_config_set(slog_config_t *pCfg) {

    if (g_slog == NULL) {
        fprintf(stderr, "Slog do not initialized correctly, please call slog_init first!\n");
        return;
    }

    if (pCfg == NULL) {
        return;
    }

    slog_lock(g_slog);
    g_slog->config = pCfg;
    slog_unlock(g_slog);
}

slog_flag_t slog_parse_flag(const char *flag) {
    if (flag == NULL) {
        fprintf(stderr, "Slog flag can not be parsed correctly!\n");
        return SLOG_UNKNOWN;
    }
    CHECK_FLAG(SLOG_ALL, flag);
    CHECK_FLAG(SLOG_TRACE, flag);
    CHECK_FLAG(SLOG_DEBUG, flag);
    CHECK_FLAG(SLOG_INFO, flag);
    CHECK_FLAG(SLOG_WARN, flag);
    CHECK_FLAG(SLOG_ERROR, flag);
    CHECK_FLAG(SLOG_FATAL, flag);
    CHECK_FLAG(SLOG_NONE, flag);
    return SLOG_UNKNOWN;
}

void slog_flag_set(slog_flag_t eFlag) {
    if (g_slog == NULL) {
        fprintf(stderr, "Slog do not initialized correctly, please call slog_init first!\n");
        return;
    }
    if (slog_get_tag(eFlag) == NULL) {
        fprintf(stderr, "Slog do not determine level correctly, support one of following levels:[%s]!\n",
                slog_get_all_levels());
        return;
    }
    slog_lock(g_slog);
    g_slog->config->nFlags = eFlag;
    slog_unlock(g_slog);
}

void slog_separator_set(const char *pFormat, ...) {
    if (g_slog == NULL) {
        fprintf(stderr, "Slog do not initialized correctly, please call slog_init first!\n");
        return;
    }
    slog_lock(g_slog);
    slog_config_t *pCfg = g_slog->config;

    va_list args;
    va_start(args, pFormat);

    if (vsnprintf(pCfg->sSeparator, sizeof(pCfg->sSeparator), pFormat, args) <= 0) {
        pCfg->sSeparator[0] = ' ';
        pCfg->sSeparator[1] = '\0';
    }

    va_end(args);
    slog_unlock(g_slog);
}

void slog_indent(uint8_t nEnable) {
    if (g_slog == NULL) {
        fprintf(stderr, "Slog do not initialized correctly, please call slog_init first!\n");
        return;
    }
    slog_lock(g_slog);
    g_slog->config->nIndent = nEnable;
    slog_unlock(g_slog);
}

void slog_new_line(uint8_t nEnable) {
    if (g_slog == NULL) {
        fprintf(stderr, "Slog do not initialized correctly, please call slog_init first!\n");
        return;
    }
    slog_lock(g_slog);
    g_slog->config->nNewLine = nEnable;
    slog_unlock(g_slog);
}

void slog_callback_set(slog_cb_t callback, void *pContext) {
    if (g_slog == NULL) {
        fprintf(stderr, "Slog do not initialized correctly, please call slog_init first!\n");
        return;
    }
    slog_lock(g_slog);
    slog_config_t *pCfg = g_slog->config;
    pCfg->pCallbackCtx = pContext;
    pCfg->logCallback = callback;
    slog_unlock(g_slog);
}

void slog_init(const char *pName, uint16_t nFlags, uint8_t nTdSafe) {
    /* Set up default values */
    g_slog = (slog_t *) malloc(sizeof(slog_t));
    g_slog->config = (slog_config_t *) malloc(sizeof(slog_config_t));
    g_slog->config->eColorFormat = SLOG_COLORING_TAG;
    g_slog->config->eDateControl = SLOG_DATE_FULL;
    g_slog->config->pCallbackCtx = NULL;
    g_slog->config->logCallback = NULL;
    g_slog->config->sSeparator[0] = ' ';
    g_slog->config->sSeparator[1] = '\0';
    g_slog->config->sFilePath[0] = '.';
    g_slog->config->sFilePath[1] = '\0';
    g_slog->config->nTraceTid = 1;
    g_slog->config->nToScreen = 1;
    g_slog->config->nNewLine = 1;
    g_slog->config->nUseHeap = 0;
    g_slog->config->nToFile = 0;
    g_slog->config->nIndent = 1;
    g_slog->config->nFlush = 0;
    g_slog->config->nFlags = nFlags;

    const char *pFileName = (pName != NULL) ? pName : SLOG_NAME_DEFAULT;
    snprintf(g_slog->config->sFileName, sizeof(g_slog->config->sFileName), "%s", pFileName);

#ifdef WIN32
    /* Enable color support */
    HANDLE hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOutput, &dwMode);
    dwMode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOutput, dwMode);
#endif

    /* Initialize mutex */
    g_slog->nTdSafe = nTdSafe;
    slog_sync_init(g_slog);
}

void slog_destroy() {
    if (g_slog == NULL) {
        return;
    }
    slog_lock(g_slog);
    free(g_slog->config);
    slog_unlock(g_slog);

    if (g_slog->nTdSafe) {
        pthread_mutex_destroy(&g_slog->mutex);
        g_slog->nTdSafe = 0;
    }
    free(g_slog);
}

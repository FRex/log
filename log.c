#include "log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


/* must sit in memory block large enough to accomodate sizeof it + length of
   text has linked list ptr as first element and text as last element */
struct log_Item {
    struct log_Item * next;
    long long timestamp1;
    long long timestamp2;
    const char * file;
    const char * func;
    int line;
    int level;
    long long tid; /* should fit any Linux pid_t or Windows DWORD */
    int len;
    char text[1]; /* keep last */
};

struct log_Logger {
    struct log_Item * head;
    int writeblocked; /* for shutdown */

    /* tid of thread that is in dump function, 0 if no one is, for error/consistency checking */
    long long dumpingthread;

    /* callbacks for output and such */
    void * outself;
    log_CallbackFunction outfunc;

    void * flushself;
    log_CallbackFlushFunction flushfunc;

    log_LevelFormatterFunction levelformatter;

};

/* a thread local for both platforms */
static __thread long long cachedtid;

#ifdef _WIN32
#include <Windows.h>

long long log_getTid(void)
{
    if(cachedtid == 0)
        cachedtid = (long long)GetCurrentThreadId();
    return cachedtid;
}

void log_milliSleep(int milliseconds)
{
    Sleep(milliseconds);
}

static void fillTimestamps(long long * t1, long long * t2)
{
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    *t1 = ft.dwLowDateTime;
    *t2 = ft.dwHighDateTime;
}

static const char * formatTimestamps(long long t1, long long t2, char * buff)
{
    FILETIME ft1, ft2;
    SYSTEMTIME st;
    ft1.dwLowDateTime = t1;
    ft1.dwHighDateTime = t2;
    if(!FileTimeToLocalFileTime(&ft1, &ft2) || !FileTimeToSystemTime(&ft2, &st))
        return strcpy(buff, "??:??:??.???????");

    sprintf(
        buff, "%02d:%02d:%02d.%07lld",
        st.wHour, st.wMinute, st.wSecond,
        t1 % (10 * 1000 * 1000) /* t1 is amount of hundreds of ns, so modulo it by 10 million */
    );
    return buff;
}
#endif /* _WIN32 */

#ifdef __linux__
#include <sys/syscall.h>
#include <unistd.h>
#include <time.h>

long long log_getTid(void)
{
    if(cachedtid == 0)
        cachedtid = (long long)syscall(SYS_gettid);
    return cachedtid;
}

void log_milliSleep(int milliseconds)
{
    usleep(milliseconds * 1000);
}

static void fillTimestamps(long long * t1, long long * t2)
{
    struct timespec spec;
    if(0 != clock_gettime(CLOCK_REALTIME, &spec))
        spec.tv_sec = spec.tv_nsec = 0;
    *t1 = spec.tv_sec;
    *t2 = spec.tv_nsec;
}

static const char * formatTimestamps(long long t1, long long t2, char * buff)
{
    struct tm tm;
    time_t t = (time_t)t1;
    if(NULL == localtime_r(&t, &tm))
        return strcpy(buff, "??:??:??.???????");

    /* div nsec by 100, since we need 7 digits, not 9, so we have count of hundreds of ns */
    sprintf(buff, "%02d:%02d:%02d.%07lld", tm.tm_hour, tm.tm_min, tm.tm_sec, t2 / 100);
    return buff;
}
#endif /* __linux__ */

void log_CallbackDumpToFILE(void * file, const void * data, int size)
{
    FILE * f = (FILE*)file;
    fwrite(data, 1, size, f); /* TODO: err check? */
}

void log_CallbackFlushFILE(void * file, int itemswritten)
{
    (void)itemswritten;
    FILE * f = (FILE*)file;
    fflush(f);
}

log_Logger * log_Logger_createForFILE(void * FILE)
{
    log_Logger * logger = log_Logger_create(&log_CallbackDumpToFILE, FILE);
    if(!logger)
        return NULL;

    log_Logger_setFlushCallback(logger, &log_CallbackFlushFILE, FILE);
    return logger;
}

log_Logger * log_Logger_create(log_CallbackFunction outfunc, void * outself)
{
    if(outfunc == NULL)
        return NULL;

    log_Logger * logger = (log_Logger*)malloc(sizeof(log_Logger));
    if(!logger)
        return NULL;

    logger->head = NULL;
    logger->writeblocked = 0;
    logger->outself = outself;
    logger->outfunc = outfunc;
    logger->dumpingthread = 0;
    return logger;
}

void log_Logger_destroy(log_Logger * logger)
{
    /* TODO: dump all data from list? or only free the list? */
    free(logger);
}

void log_Logger_setLevelFormatter(log_Logger * logger, log_LevelFormatterFunction formatter)
{
    logger->levelformatter = formatter;
}

void log_Logger_setFlushCallback(log_Logger * logger, log_CallbackFlushFunction flushfunc, void * flushself)
{
    logger->flushfunc = flushfunc;
    logger->flushself = flushself;
}

void log_Logger_shutdown(log_Logger * logger)
{
    (void)logger;
    /* TODO: just free the list? dump it first or not? */
    fprintf(stderr, "log_Logger_shutdown - NOT IMPLEMENTED\n");
    fflush(stderr);
}

void log_Logger_blockWrite(log_Logger * logger)
{
    __atomic_store_n(&logger->writeblocked, 1, __ATOMIC_SEQ_CST);
}

int log_Logger_isWriteBlocked(const log_Logger * logger)
{
    return __atomic_load_n(&logger->writeblocked, __ATOMIC_SEQ_CST);
}

/* add a log text line with file, line, func and log level formatted in */
void log_Logger_logStr(log_Logger * logger, const char * file, int line, const char * func, int level, const char * text)
{
    log_Logger_logLen(logger, file, line, func, level, text, strlen(text));
}

void log_Logger_logLen(log_Logger * logger, const char * file, int line, const char * func, int level, const char * text, int len)
{
    if(len <= 0)
        return;

    /* +1 is for \n to add at the end */
    struct log_Item * item = (struct log_Item*)malloc(sizeof(struct log_Item) + len + 1);
    item->next = NULL;
    fillTimestamps(&item->timestamp1, &item->timestamp2);
    item->file = file;
    item->line = line;
    item->func = func;
    item->level = level;
    item->len = len + 1; /* +1 is for \n added at the end */
    memcpy(item->text, text, len);
    item->text[len] = '\n';
    item->tid = log_getTid();
    while(!__atomic_compare_exchange_n(&logger->head, &item->next, item, 1, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
}

static struct log_Item * reverseList(struct log_Item * head)
{
    struct log_Item * ret = NULL;
    while(head)
    {
        struct log_Item * next = head->next;
        head->next = ret;
        ret = head;
        head = next;
    } /* while head */
    return ret;
}

/* returns amount of items written */
int log_Logger_dumpAll(log_Logger * logger)
{
    char buff[10 * 1024];
    char timestampbuff[1024];
    struct log_Item * prev;
    struct log_Item * list;

    /* if not 0 it means someone is already dumping - this is an API misuse */
    const long long oldtid = __atomic_exchange_n(&logger->dumpingthread, log_getTid(), __ATOMIC_SEQ_CST);
    if(oldtid != 0)
        abort();

    list = __atomic_exchange_n(&logger->head, NULL, __ATOMIC_SEQ_CST);
    list = reverseList(list);
    int ret = 0;
    while(list)
    {
        const int len = snprintf(
            buff, sizeof(buff),
            "%s %s:%d %s [%d] tid: %lld ",
            formatTimestamps(list->timestamp1, list->timestamp2, timestampbuff),
            list->file, list->line, list->func,
            list->level,
            list->tid
        );

        if(len > 0)
            logger->outfunc(logger->outself, buff, len);

        logger->outfunc(logger->outself, list->text, list->len);

        /* get next elements and free this one */
        prev = list;
        list = list->next;
        free(prev);
        ++ret;
    } /* while list */

    if(logger->flushfunc)
        logger->flushfunc(logger->flushself, ret);

    /* set back to 0 so another thread can use this object too, if library client does own locking to ensure correctness */
    __atomic_store_n(&logger->dumpingthread, 0, __ATOMIC_SEQ_CST);
    return ret;
}

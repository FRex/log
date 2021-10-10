#pragma once

/* helper functions that are OS dependent */
long long log_getTid(void);
void log_milliSleep(int milliseconds);

/* callbacks for formatting level integer, and for output and flush */
typedef void(*log_CallbackFunction)(void * self, const void * data, int size);
typedef const char*(*log_LevelFormatterFunction)(int level);
typedef void(*log_CallbackFlushFunction)(void * self, int itemswritten);

/* callbacks that use a FILE ptr form C stdio and fwrite/flush */
void log_CallbackDumpToFILE(void * file, const void * data, int size);
void log_CallbackFlushFILE(void * file, int itemswritten);

/* forward declaration (definition is lower) */
typedef struct log_Logger log_Logger;

void log_Logger_init(log_Logger * logger, log_CallbackFunction outfunc, void * outself);
void log_Logger_setLevelFormatter(log_Logger * logger, log_LevelFormatterFunction formatter);
void log_Logger_setFlushCallback(log_Logger * logger, log_CallbackFlushFunction flushfunc, void * flushself);
void log_Logger_shutdown(log_Logger * logger);

/* set/get the one time flag that denies writing */
void log_Logger_blockWrite(log_Logger * logger);
int log_Logger_isWriteBlocked(const log_Logger * logger);

/* add a log text line with file, line, func and log level formatted in */
void log_Logger_logStr(log_Logger * logger, const char * file, int line, const char * func, int level, const char * text);
void log_Logger_logLen(log_Logger * logger, const char * file, int line, const char * func, int level, const char * text, int len);

/* returns amount of items written */
int log_Logger_dumpAll(log_Logger * logger);

/* PUBLIC API END */

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

    /* callbacks for output and such */
    void * outself;
    log_CallbackFunction outfunc;

    void * flushself;
    log_CallbackFlushFunction flushfunc;

    log_LevelFormatterFunction levelformatter;

};

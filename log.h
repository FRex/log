#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* helper functions that are OS dependent */
long long log_getTid(void);
void log_milliSleep(int milliseconds);

/* 1 unit = 100 nanoseconds, UTC timestmap, but the starting point is OS
   dependent so the only intended use is to format it into local time using
   a 30 character buffer, the format is HH:MM:SS.xxxxxxx, hours, minutes,
   seconds, and 7 digits of precision for sub-seconds (count of 100s ns) */
unsigned long long log_getPreciesTimestamp(void);
char * log_formatPreciseTimestampAsLocalTime(char * buff30chars, unsigned long long timestamp);

/* callbacks for formatting level integer, and for output and flush */
typedef void(*log_CallbackFunction)(void * self, const void * data, int size);
typedef const char*(*log_LevelFormatterFunction)(int level);
typedef void(*log_CallbackFlushFunction)(void * self, int itemswritten);

/* predefined callbacks that use a FILE ptr from C stdio and fwrite/flush */
void log_CallbackDumpToFILE(void * file, const void * data, int size);
void log_CallbackFlushFILE(void * file, int itemswritten);

/* forward declaration (this is an opaque handle) */
typedef struct log_Logger log_Logger;

/* create or destroy the object, first version is a shortcut that write to given stdlib FILE ptr */
log_Logger * log_Logger_createForFILE(void * FILE);
log_Logger * log_Logger_create(log_CallbackFunction outfunc, void * outself);
void log_Logger_destroy(log_Logger * logger);

/* set extra callbacks (they are optional) */
void log_Logger_setLevelFormatter(log_Logger * logger, log_LevelFormatterFunction formatter);
void log_Logger_setFlushCallback(log_Logger * logger, log_CallbackFlushFunction flushfunc, void * flushself);

/* set/get the one time flag that denies further writing so one more dumpAll will finish the output */
void log_Logger_blockWrite(log_Logger * logger);
int log_Logger_isWriteBlocked(const log_Logger * logger);

/* add a log text line with file, line, func and log level formatted in, safe to call from any thread */
void log_Logger_logStr(log_Logger * logger, const char * file, int line, const char * func, int level, const char * text);
void log_Logger_logLen(log_Logger * logger, const char * file, int line, const char * func, int level, const char * text, int len);

/* this functions allows avoiding doing a syscall in client threads, its safe
   to call from any thread but needs to be called extremely often to maintain
   timestamp accuracy and precision, e.g. in a thread that does nothing but
   calls this and log_getPreciesTimestamp, calling with 0 makes the logger
   do syscalls itself on each log line call again */
void log_Logger_setTimestamp(log_Logger * logger, unsigned long long timestamp);

/* write out the elements, safe to call from one thread at once, returns amount of items written */
int log_Logger_dumpAll(log_Logger * logger);

#ifdef __cplusplus
} /* extern "C" end */
#endif

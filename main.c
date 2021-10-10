#include <pthread.h>
#include <stdio.h>
#include "log.h"

static void * tmain(void * arg)
{
    log_Logger * logger = (log_Logger*)arg;
    int amount = 100;
    for(int i = 0; i < amount; ++i)
    {
        log_Logger_logStr(logger, __FILE__, __LINE__, __func__, 0, "test text aaaa");
        log_milliSleep(100);
    }

    return NULL;
}

static void * logmain(void * arg)
{
    log_Logger * logger = (log_Logger*)arg;
    while(1)
    {
        const int dumped = log_Logger_dumpAll(logger);
        /* if its blocked and we didnt dump anything just do last dump and quit */
        if(dumped == 0 && log_Logger_isWriteBlocked(logger))
        {
            log_Logger_dumpAll(logger);
            break;
        } /* if */

        /* todo: some adjust of sleep time based on amount of messages written */
        log_milliSleep(100);
    } /* while 1*/
    return NULL;
}

#define doslow(expr) do{fprintf(stderr, "Doing '%s' ... ", #expr); fflush(stderr); expr; fprintf(stderr, "done!\n");}while(0)

int main()
{
    pthread_t logthread;
    pthread_t threads[10];
    int workers = 10;
    log_Logger * logger = log_Logger_createForFILE(stdout);

    pthread_create(&logthread, NULL, logmain, logger);

    for(int i = 0; i < workers; ++i)
        pthread_create(&threads[i], NULL, tmain, logger);

    for(int i = 0; i < workers; ++i)
        pthread_join(threads[i], NULL);

    log_Logger_blockWrite(logger);
    pthread_join(logthread, NULL);
    log_Logger_destroy(logger);
}

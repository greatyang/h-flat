#ifndef DEBUG_H_
#define DEBUG_H_

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

/*
 *      void *caller[2];
        backtrace (caller, 2);
        char ** strings = backtrace_symbols (caller, 2);
        strings[1][strchr(strchr(strings[1],'p'),'P') - strings[1] ] = '\0';
        pok_trace("%s > LOOKUP %s  ->  %s", strchr(strings[1],'p'),
                        user_path, key.c_str());
 *
 */

static int llconfig = 0;

#define pok_trace(message...)   pok_printlog(1, __FUNCTION__, __FILE__, __LINE__, ## message)
#define pok_debug(message...)   pok_printlog(2, __FUNCTION__, __FILE__, __LINE__, ## message)
#define pok_warning(message...) pok_printlog(3, __FUNCTION__, __FILE__, __LINE__, ## message)
#define pok_error(message...)   pok_printlog(4, __FUNCTION__, __FILE__, __LINE__, ## message)

#define REQ(fun) if(fun){ pok_error("Unrecoverable File System Error. \n Killing myself now. Goodbye."); if(PRIV)delete PRIV; exit(EXIT_FAILURE); }

static void pok_printlog(const int loglevel, const char* fun, const char* file, int line, const char* msg, ...)
{
    if (loglevel < llconfig)
        return;

    time_t rawtime;
    time(&rawtime);
    struct tm * timeinfo = localtime(&rawtime);
    printf("[%d:%d:%d] ", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    if (loglevel == 2)
        printf("\e[0;37m DEBUG ");
    if (loglevel == 3)
        printf("\e[0;33m WARNING ");
    if (loglevel == 4)
        printf("\e[0;31m ERROR ");

    std::string filename(file);
    filename.erase(0, filename.find_last_of('/') + 1);

    printf("\e[033;32m%s@%s\e[033;34m(%d) \e[033;39m \t", fun, filename.c_str(), line);

    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);

    printf("\n");
}

#endif

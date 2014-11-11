
/* h-flat file system: Hierarchical Functionality in a Flat Namespace
 * Copyright (c) 2014 Seagate
 * Written by Paul Hermann Lensing
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef DEBUG_H_
#define DEBUG_H_

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

static int llconfig = 0;

#define hflat_trace(message...)   hflat_printlog(1, __FUNCTION__, __FILE__, __LINE__, ## message)
#define hflat_debug(message...)   hflat_printlog(2, __FUNCTION__, __FILE__, __LINE__, ## message)
#define hflat_warning(message...) hflat_printlog(3, __FUNCTION__, __FILE__, __LINE__, ## message)
#define hflat_error(message...)   hflat_printlog(4, __FUNCTION__, __FILE__, __LINE__, ## message)

#define REQ_TRUE(fun) if(fun == false){ hflat_error("Unrecoverable File System Error. \n Killing myself now. Goodbye."); if(PRIV)delete PRIV; exit(EXIT_FAILURE); }
#define REQ_0(fun) REQ_TRUE((fun == 0))


static void hflat_printlog(const int loglevel, const char* fun, const char* file, int line, const char* msg, ...)
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

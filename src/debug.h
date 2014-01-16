#ifndef DEBUG_H_
#define DEBUG_H_

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

static int llconfig = 0;

#define pok_trace(message...) 	pok_printlog(1, __FUNCTION__, __FILE__, __LINE__, ## message)
#define pok_debug(message...) 	pok_printlog(2, __FUNCTION__, __FILE__, __LINE__, ## message)
#define pok_warning(message...) pok_printlog(3, __FUNCTION__, __FILE__, __LINE__, ## message)
#define pok_error(message...)   pok_printlog(4, __FUNCTION__, __FILE__, __LINE__, ## message)

#define kill_compound_fail() { 																	\
pok_error(	"Unrecoverable File System Error. \n "												\
			"Failed undoing a compound file system operation that suceeded only partially. \n"	\
			"Killing myself now. Goodbye.");													\
delete PRIV;																					\
std::exit(EXIT_FAILURE);																		\
}


static void pok_printlog(const int loglevel, const char* fun, const char* file, int line, const char* msg, ...)
{
	if(loglevel<llconfig) return;

    time_t rawtime;
	time (&rawtime);
	struct tm * timeinfo = localtime ( &rawtime );
	printf("[%d:%d:%d] ",timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);

	if(loglevel==2)	printf("\e[0;37m DEBUG ");
	if(loglevel==3)	printf("\e[0;33m WARNING ");
	if(loglevel==4) printf("\e[0;31m ERROR ");

	printf("\e[033;32m%s@%s\e[033;34m(%d) \e[033;39m \t",
			fun, file, line
		   );

	va_list args;
	va_start(args, msg);
	vprintf(msg, args);
	va_end(args);

	printf("\n");
}

#endif

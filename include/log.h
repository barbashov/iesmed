/*
 * 19 Nov 2006
 * Barbashov Ilya for Info-Tel
 * barbashov.ilya@gmail.com
 */

#ifndef __LOG_H_
#define __LOG_H_

#include <ctime>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


#include "constants.h"
#include "globals.h"
#include "mutex.h"

//#define __DEBUG
#ifdef __DEBUG
#define DEBUG_LOG( b ) { printf( b ); }
#else
#define DEBUG_LOG( b )
#endif

#define LOG_DEFAULT_PREFIX "logs/iesme"
#define LOG_EXTENSION ".log"


class log_t
{
public:
	log_t( unsigned char );
	~log_t( void );
	
	int lprint( const char *, ... );
	int set_behaviour( bool );
	
	bool log_rotation;
	
private:
	void log_rotate( void );
	inline void log_get_date_string( char * buffer, bool append_time = true );
	
	char * log_filename;
	FILE * log_file;
	int log_fd;
	time_t log_lastrotate;
	unsigned char esmeid;
	
	mutex * m_log;
};

#endif

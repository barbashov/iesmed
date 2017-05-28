#include "log.h"

log_t::log_t( unsigned char log_esmeid )
	: esmeid( log_esmeid ),
	  log_rotation( true ),
	  log_lastrotate( 0 ),
	  log_file( NULL ),
	  log_fd(-1)
{
	log_filename = new char[SZ_BUFFER_SIZE];
	m_log = new mutex();
	
	log_rotate();
}

log_t::~log_t( void )
{
	if ( log_file && log_fd > -1 )
	{
		m_log->lock();
		fclose ( log_file );
	}
	
	DEBUG_LOG( "~log_t: deleting m_log\n" );
	delete m_log;
	DEBUG_LOG( "~log_t: deleting log_filename\n" );
	delete[] log_filename;
}

void
log_t::log_rotate( void )
{
	time_t curtime;
	time( &curtime );
	
	if ( log_lastrotate == 0  || difftime( curtime, log_lastrotate ) >= SECONDS_IN_DAY )
	{
		struct tm * cur_time = localtime( &curtime );
		char sztmp[SZ_BUFFER_SIZE];
		
		strftime( sztmp, SZ_BUFFER_SIZE, "%%s%%d_%Y_%m_%d%%s", cur_time );
		sprintf( log_filename, sztmp, LOG_DEFAULT_PREFIX, esmeid, LOG_EXTENSION );
		
		cur_time->tm_sec = 0;
		cur_time->tm_min = 0;
		cur_time->tm_hour = 0;
		log_lastrotate = mktime( cur_time );
		
		m_log->lock();
		if ( log_file && log_fd > -1 )
		{
			fclose( log_file );
			close(log_fd);
		}
		
		log_fd = open( log_filename, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH );
		if (errno)
			perror("open()");
		fchmod( log_fd, 00664 );
		log_file = fdopen( log_fd, "a" );
		setvbuf( log_file, NULL, _IONBF, 0 );
		if (errno)
			perror("fdopen()");
		m_log->unlock();
		
		DEBUG_LOG( "Log opened\n" )
	}
}

int 
log_t::lprint( const char * szevent, ... )
{
	if ( this->log_file )
	{
		char sztmp[SZ_BUFFER_SIZE];
		char sztext[SZ_BUFFER_SIZE];
		va_list args;
		
		va_start( args, szevent );
		vsprintf( sztext, szevent, args );
		va_end( args );
		
		m_log->lock();
		log_get_date_string( sztmp );

		if ( log_rotation )
			log_rotate();
		
		fprintf( log_file, sztmp, sztext );
		m_log->unlock();
		
#ifdef __DEBUG
		char szdbg[1024];
		sprintf( szdbg, sztmp, sztext );
		DEBUG_LOG( szdbg )
#endif
	}
	
	return 0;
}

inline void 
log_t::log_get_date_string( char * buffer, bool append_time )
{
	time_t ttime = time( NULL );
	struct tm * ttm = localtime( &ttime );
	strftime( buffer, SZ_BUFFER_SIZE, "[%Y-%m-%d %H:%M:%S] %%s\n", ttm );
}

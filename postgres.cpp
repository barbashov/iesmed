#include "postgres.h"

postgres::postgres( const char * szhost, const char * szdb, const char * szuser, const char * szpasswd, log_t * p_esmelog )
	: conn_open( false ),
	  db_trigger( NULL ),
	  esmelog( p_esmelog )
{
	DEBUG_LOG( "postgres::postgres() called\n" )
	
	char * sztmp = new char[255];
	sprintf( sztmp, "host=%s dbname=%s user=%s password=%s", szhost, szdb, szuser, szpasswd );
	
	DEBUG_LOG( "trying to connect\n" )
	try
	{
		db_conn = new connection( sztmp );
	}
	catch ( ... )
	{
		esmelog->lprint( "[-] Couldn't connect to database!" );
		return;
	}
	delete[] sztmp;
	
	conn_open = true;
	
	m_db = new mutex();
	//printf( "m_db address: 0x%08X\n", m_db );
	szfquery = new char[SZ_BUFFER_SIZE];	
}

postgres::~postgres()
{
	db_conn->disconnect();
	remove_listener();
	
	delete db_conn;
	delete m_db;
	delete[] szfquery;
}

void
postgres::disconn()
{
	db_conn->disconnect();
}

int 
postgres::query( const char * szquery, ... )
{
	if ( !conn_open ) {
		esmelog->lprint( "[-] postgres::query: Database connection closed." );
		return IESME_ERROR_FAILED;
	}
	
	int resid = find_first_unused_result();
	if ( resid == IESME_NO_MEMORY ) {
		esmelog->lprint( "[-] postgres::query: No memory." );
		return( IESME_NO_MEMORY );
	}
	
	char * szexception = new char[SZ_BUFFER_SIZE];
	bool db_exception = false;
	result * pR;
	va_list args;

	va_start( args, szquery );
	vsprintf( szfquery, szquery, args );
	va_end( args );
	
	m_db->lock();
	try
	{
		transaction<serializable> trans( (*db_conn), "Common" );
		pR = new result;
		(*pR) = trans.exec( szfquery );
		
		trans.commit();
	}
	catch ( const exception& e )
	{
		db_exception = true;
		strcpy( szexception, e.what() );	
	}
	catch ( ... )
	{
		db_exception = true;
		strcpy( szexception, "Unhalted exception!" );
	}
	
	if ( db_exception )
	{
		freeresult( resid );
		esmelog->lprint( "[-] Query \"%s\" failed. pgSQL said: %s", szfquery, szexception );

		m_db->unlock();
		return( IESME_ERROR_FAILED );
	}
	
//	esmelog->lprint( "[+] Query \"%s\" successed." );

	results[resid].resize( pR->size() );
	for ( size_t i = 0; i < pR->size(); ++i )
	{
		const tuple &rT = pR->at( i );
		for ( size_t j = 0; j < rT.size(); ++j )
		{
			results[resid][i][ rT[j].name() ] = const_cast<char *>( rT[j].c_str() );
		}
	}
	
	iterators[resid] = 0;
	delete pR;
	delete[] szexception;
	
	m_db->unlock();
	
	return( resid );
}

int 
postgres::freeresult( unsigned int resid )
{
	m_db->lock();
	try {
		results[resid].clear();
	}
	catch ( ... ) {
		esmelog->lprint( "[-] Error while deleting result #%d", resid );
		m_db->unlock();
		return( IESME_ERROR_FAILED );
	}
	
	m_db->unlock();
	return( IESME_ERROR_OK );
}

long
postgres::find_first_unused_result( void )
{
	m_db->lock();
	for ( unsigned int i = 0; i < POSTGRES_MAX_RESULTS; i++ )
	{ 
		if ( results[i].empty() )
		{
			m_db->unlock();
			return( i );
		}
	}
	m_db->unlock();
	
	return( IESME_NO_MEMORY );
}

pgrow_t *
postgres::fetch_row( unsigned int resid )
{
	m_db->lock();
	pgrow_t * ret;
	try {
		ret = &(results[resid]).at( iterators[resid]++ );
	} catch( const exception& e ) {
		freeresult(resid);
		m_db->unlock();
		return( NULL );
	}
	m_db->unlock();

	return( ret );

/*	if ( results[resid].size() > 0 && iterators[resid] >= results[resid].size() )
	{
		freeresult( resid );
		m_db->unlock();
		return( NULL );
	}
	
	m_db->unlock();
	return( &(results[resid]).at( iterators[resid]++ ) ); */
}

size_t
postgres::num_rows( unsigned int resid )
{
	return( results[resid].size() );
}

int
postgres::create_listener( const char * name )
{
	if ( !conn_open )
		return IESME_ERROR_FAILED;
	
	char * szfquery = new char[SZ_BUFFER_SIZE];
	char * szexception = new char[SZ_BUFFER_SIZE];
	bool db_exception = false;
	
	sprintf( szfquery, "LISTEN %s", name );
	
	m_db->lock();
	try
	{
		transaction<> trans( (*db_conn), "Listener" );
		trans.exec( szfquery );
		trans.commit();
	}
	catch ( const exception &e )
	{
		db_exception = true;
		strcpy( szexception, e.what() );
	}
	catch ( ... )
	{
		db_exception = true;
		strcpy( szexception, "Unhalted exception!" );
	}
	
	if ( db_exception )
	{
		esmelog->lprint( "[-] Failed to create listener for notify \"%s\". pgSQL said: %s", name, szexception );
		m_db->unlock();
		return( IESME_ERROR_FAILED );
	}
	
	delete[] szexception;
	delete[] szfquery;
	
	db_trigger = new pg_trigger_t( (*db_conn), name );
	
	m_db->unlock();
	
	return( IESME_ERROR_OK );
}

int
postgres::remove_listener()
{
	m_db->lock();
	if ( db_trigger )
	{
		delete db_trigger;
		db_trigger = NULL;
		
		m_db->unlock();	
		return( IESME_ERROR_OK );
	}
	m_db->unlock();
	
	return( IESME_ERROR_FAILED );
}

int
postgres::listen( time_t _timeout = 0 )
{
	try {
		if ( _timeout )
			db_conn->await_notification( _timeout, 0 );
		else
			db_conn->await_notification();
	} catch (...) {
		return -1;
	}
	
	return( db_trigger->done() );
}

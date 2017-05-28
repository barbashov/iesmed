/*
 * 19 Nov 2006
 * Barbashov Ilya for Info-Tel
 * barbashov.ilya@gmail.com
 */

#ifndef __POSTGRES_H__
#define __POSTGRES_H__

#include <pqxx/pqxx>
using namespace pqxx;

#include <stdarg.h>
#include <string>
#include <vector>
#include <map>
using namespace std;

#include "types.h"
#include "constants.h"
#include "log.h"
#include "errors.h"
#include "mutex.h"

typedef result::tuple tuple;

typedef map<string,string> pgrow_t;
typedef pgrow_t::iterator pgrowi_t;
typedef vector<pgrow_t> pgres_t;
typedef pgres_t::iterator pgresi_t;

#define POSTGRES_MAX_RESULTS	32
#define DB_HOST			"localhost"
#define DB_NAME			"smdb"
#define DB_USER			"sms"
#define DB_PASSWORD		"CVCgfhjkm"
#define OUTCOMING_NOTIFY	"submit_sm"

class postgres
{
private:
	class pg_trigger_t : public trigger
	{
	public:
		explicit pg_trigger_t( connection &C, const char * n_name )
			: trigger( C, n_name ),
			  m_done( 0 )
		{
			DEBUG_LOG( "trigger created\n" )
		}
		virtual void operator()( int be_pid )
		{
			DEBUG_LOG( "recieved some notification\n" )
			m_done = 1;
		}
		int done() const
		{
			return( m_done );
		}
	private:
		int m_done;
	};
	
public:
	postgres( const char *, const char *, const char *, const char *, log_t * );
	~postgres();
	
	void disconn();
	
	int query( const char *, ... );
	int freeresult( unsigned int );
	pgrow_t * fetch_row( unsigned int );
	size_t num_rows( unsigned int );
	
	int create_listener( const char * );
	int remove_listener();
	int listen( time_t );
	
	bool conn_open;
	
private:
	connection * db_conn;
	log_t * esmelog;
	pgres_t results[POSTGRES_MAX_RESULTS];
	size_t iterators[POSTGRES_MAX_RESULTS];
	
	long find_first_unused_result( void );
	
	mutex * m_db;
	
	pg_trigger_t * db_trigger;
	
	char * szfquery;
};

#endif

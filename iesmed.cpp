#include "iesmed.h"

/*
 * iesmed constructor
 */
iesmed::iesmed( int argc, char ** argv )
	: esmeid(0),
	  debug(false),
	  background(true),
	  iesmed_alive(true),
	  init_failed(true),
	  pid_fd(-1),
	  smscs_count(0),
	  smscs(NULL),
	  g_tlv_map(NULL),
	  tlv_max_tag(0),
	  timer_enabled(false)
{
	DEBUG_LOG( "iesmed constructor called\n" );

	pid_fname = new char[SZ_BUFFER_SIZE];
	
	DEBUG_LOG( "parsing args\n" )
	proceed_args( argc, argv );
	
	/*
	 * !!!!!!!!!!!!!!!!!!!!!!!!!!!
	 */
//	esmeid = 6;
	/*
	 * !!!!!!!!!!!!!!!!!!!!!!!!!!!
	 */
	 
	if ( esmeid == 0 )
	{
		errx( IESME_ERROR_FAILED, "There is no esmeid!" );
	}
	
	DEBUG_LOG( "creating log\n" )
	esmelog = new log_t( esmeid );
	DEBUG_LOG( "logging init event\n" )
	esmelog->lprint( "\t[+] iESMEd %d.%d%s init", VERSION_MAJOR, VERSION_MINOR, VERSION_SUFFIX );

	DEBUG_LOG( "creating db\n" )
	db = new postgres( DB_HOST, DB_NAME, DB_USER, DB_PASSWORD, this->esmelog );
	
	if ( ! db->conn_open )
		return;
	
	if ( this->background )
		this->do_fork();
	
	sprintf( pid_fname, "/var/run/iesmed/iesmed_%d.pid", esmeid );
	DEBUG_LOG( "writing pidfile\n" )

	if ( write_pidfile() )
		return;
		
	init_failed = false;
}

iesmed::~iesmed()
{
	iesmed_alive = false;
	
	esmelog->lprint( "[+] iESMEd %d.%d%s terminating\n", VERSION_MAJOR, VERSION_MINOR, VERSION_SUFFIX );
	if ( pid_fd >= 0 )
	{
		unlink(pid_fname);
		close(pid_fd);
	}

	DEBUG_LOG( "deleting smscs\n" );
	for ( uint32 i = 0; i < smscs_count; ++i )
	{
		DEBUG_LOG( "deleting smsc\n" );
		if ( smscs[i] != NULL )
			delete smscs[i];
	}
	free( smscs );
	
	DEBUG_LOG( "deleting db\n" );
	delete db;	
	DEBUG_LOG( "deleting esmelog\n" );
	delete esmelog;
	DEBUG_LOG( "deleting pid_fname[]\n" );
	delete[] pid_fname;
	
	if ( g_tlv_map != NULL )
	{
		DEBUG_LOG( "deleting g_tlv_map\n" );
		for( size_t i = 0; i <= tlv_max_tag; i++ )
		{
			if ( g_tlv_map[i].description != NULL )
				free(g_tlv_map[i].description);
			if ( g_tlv_map[i].column_name != NULL )
				free(g_tlv_map[i].column_name);
		}
		free( g_tlv_map );
	}
}

int 
iesmed::start()
{	
	if ( db_read_tlv_map() )
		return( IESME_ERROR_FAILED );
		
	if ( db_read_smscs() )
		return( IESME_ERROR_FAILED );
	
	smsc_init_constants();
	
	esmelog->lprint( "[+] Parent goes in monitor mode." );
	
	while ( iesmed_alive )
	{
		if ( !smscs_count )
		{
			esmelog->lprint( "[+] There is no SMSCs left and nothing to do." );
			break;
		}
		
		for ( ssize_t i = 0; i < smscs_count; ++i )
		{
			switch( smscs[i]->get_state() )
			{
				// TODO fillup with various states
				case SMSC_STATE_CLOSED_BY_ERR:
				case SMSC_STATE_CLOSED_NEED_CONNECT:
					smscs[i]->conn();
					break;
				
				case SMSC_STATE_AWAITING_START:
					esmelog->lprint( "[+] Starting SMSC #%d.", smscs[i]->smsc_id() );
					if ( smscs[i]->start() == IESME_ERROR_OK )
					{
						esmelog->lprint( "[+] SMSC #%d started successfully.", smscs[i]->smsc_id() );
						break;
					}
					else
					{
						esmelog->lprint( "[-] SMSC #%d failed to start. Erasing.", smscs[i]->smsc_id() );
					}

				case SMSC_STATE_AWAITING_DESTROY:
					esmelog->lprint( "[+] Shutting down and erasing SMSC #%d.", smscs[i]->smsc_id() );
					DEBUG_LOG( "deleting\n" );
					timer_enabled = false;
					delete smscs[i];
					DEBUG_LOG( "erasing\n" );
					if ( smscs_count > 1 && i != (smscs_count - 1) )
					{
						memmove( \
							(void *)(smscs + i * sizeof(smsc *)), \
							(void *)(smscs + (i + 1) * sizeof(smsc *)), \
							(smscs_count-- - i--) * sizeof(smsc *) \
						);
					}
					else
					{
						--smscs_count;
						--i;
					}
					timer_enabled = true;
						
					DEBUG_LOG( "erased\n" );
					break;
			}
		}
		
		usleep( PARENT_USLEEP );
	}

	return( IESME_ERROR_OK );
}

int
iesmed::db_read_smscs()
{
	esmelog->lprint( "[+] Taking SMSC information" );
		
	int result;
	if ( (result = db->query( "SELECT * FROM smsc WHERE esmeid = %d AND inuse", esmeid )) < 0 )
	{
		esmelog->lprint( "[-] Cannot get SMSC information. Aborting." );
		return( IESME_ERROR_FAILED );
	}
	
	if ( !(smscs_count = db->num_rows( result )) )
	{
		esmelog->lprint( "[-] No SMSCs for esmeid %d. Aborting.", esmeid );
		db->freeresult( result );
		return( IESME_ERROR_FAILED );
	}
	
	smscs = (smsc **)malloc( sizeof(smsc *) * smscs_count );
	
	size_t smsc_current = 0;
	pgrow_t* prow = NULL;
	while ( prow = db->fetch_row( result ) )
	{
		pgrow_t& row = *prow;
		
		smscopts_t tsmscopts;
		if ( !fill_smscopts( row, &tsmscopts ) )
		{
			esmelog->lprint( "[-] Failed to proceed SMSC. Skipping." );
			continue;
		}
		
		DEBUG_LOG( "starting smsc\n" );
		
		smscs[smsc_current++] = new smsc( &tsmscopts, esmelog, esmeid, g_tlv_map, tlv_max_tag );
	}
	smscs_count = smsc_current;
	timer_enabled = true;
	
	esmelog->lprint( "[+] Processed %d SMSCs, starting them.", smsc_current );
	
	return( IESME_ERROR_OK );
}

int
iesmed::db_read_tlv_map()
{
	esmelog->lprint( "[+] Reading TLV map." );

	int result;
	pgrow_t * prow = NULL;
	
	bool err = true;
	do
	{
		if ( (result = db->query( "SELECT MAX(tag) AS max_tag FROM tlv_map" )) < 0 )
			break;
			
		prow = db->fetch_row(result);
		;
		
		if ( ! (tlv_max_tag = atoi( (*prow)["max_tag"].c_str() )) )
		{
			esmelog->lprint( "[-] Strange: MAX(tag) for tlv_map is zero. Is tlv_map empty?" );
			break;
		}
		
		if ( (result = db->query( "SELECT * FROM tlv_map" )) < 0 )
			break;
		
		err = false;
	}
	while (0);
	if ( err )
	{
		esmelog->lprint( "[-] Cannot read TLV map from DB. Aborting" );
		return( IESME_ERROR_FAILED );
	}
		
	esmelog->lprint( "[+] Allocating %d bytes of memory and starting load tlv_map.", sizeof(tlv_map_t) * (tlv_max_tag + 1) );

	g_tlv_map = (tlv_map_t *)malloc( sizeof(tlv_map_t) * (tlv_max_tag + 1) );
	
	for ( size_t i = 0; i <= tlv_max_tag; i++ )
	{
		g_tlv_map[i].description = NULL;
		g_tlv_map[i].column_name = NULL;
	}
	
	
	uint16 _tag;
	while ( prow = db->fetch_row( result ) )
	{
		pgrow_t& row = *prow;
		
		_tag = atoi(row["tag"].c_str());
		g_tlv_map[_tag].store_as = atoi(row["store_as"].c_str());
		g_tlv_map[_tag].value_type = atoi(row["value_type"].c_str());
		
		g_tlv_map[_tag].description = (char *)malloc( sizeof(char) * ( strlen(row["description"].c_str()) + 1 ) );
		g_tlv_map[_tag].column_name = (char *)malloc( sizeof(char) * ( strlen(row["column_name"].c_str()) + 1 ) );
		strcpy( g_tlv_map[_tag].column_name, row["column_name"].c_str() );
		strcpy( g_tlv_map[_tag].description, row["description"].c_str() );
		
		if ( g_tlv_map[_tag].store_as > 3 || g_tlv_map[_tag].value_type > 4 )
		{
			esmelog->lprint( "[-] Rule for TLV with tag 0x%04X seems ambiguous. Skipping.", _tag );
			continue;
		}
		
//		esmelog->lprint( \
			"[+] Got TLV rule for tag 0x%04X (%s): store %s value in '%s' as %s", \
			_tag, \
			g_tlv_map[_tag].description, \
			c_tlv_value_type[g_tlv_map[_tag].value_type], \
			g_tlv_map[_tag].column_name, \
			c_tlv_store_as[g_tlv_map[_tag].store_as] \
		);
	}
	
	esmelog->lprint( "[+] Successed reading TLV map." );

	return IESME_ERROR_OK;
}

void 
iesmed::do_fork()
{
	if ( (this->pid = fork()) < 0 )
		errx( EXIT_FAILURE, "fork() error!\n" );
	
	if ( this->pid != 0 )
	{
		forked = false;
		DEBUG_LOG( "Terminating parent process\n" );
		exit( EXIT_SUCCESS );
	}
	
	if ( setsid() < 0 )
	{
		errx( EXIT_FAILURE, "setsid() error!\n" );
	}
	
	forked = true;
}

int 
iesmed::write_pidfile( void )
{
	DEBUG_LOG( "write_pidfile started\n" )
	
	bool ret = false;
	char* pid_buf = new char[SZ_BUFFER_SIZE];
	int pid_len;
	
	do
	{
		DEBUG_LOG( "opening\n" )
		this->pid_fd = open( this->pid_fname, \
					O_CREAT | O_WRONLY, \
//					S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH
					00664
				);
		if ( this->pid_fd < 0 )
		{
			esmelog->lprint( "[-] Can't open pid file: %s", this->pid_fname );
			warn( "Can't open pid file: %s", this->pid_fname );
			break;
		}
		fchmod( pid_fd, 00664 );
	
		DEBUG_LOG( "locking\n" )
		if ( flock( this->pid_fd, LOCK_EX | LOCK_NB ) < 0 )
		{
			if ( errno == EWOULDBLOCK )
			{
				esmelog->lprint( "[-] Pid file \"%s\" locked. Already running?", this->pid_fname );
				warnx( "Already running?" );
			}
			else
			{
				esmelog->lprint( "[-] Cannot lock pid file \"%s\"", this->pid_fname );
				warn( "flock() failed: %s", this->pid_fname );
			}
			break;
		}
		
		DEBUG_LOG( "getting length\n" )
		pid_len = sprintf( pid_buf, "%d\n", getpid() );
		
		DEBUG_LOG( "truncating\n" )
		if ( ftruncate( this->pid_fd, (off_t)pid_len ) < 0 )
		{
			warn( "ftruncate() failed" );
			break;
		}
		
		DEBUG_LOG( "writing\n" )
		if ( write( this->pid_fd, (void *)pid_buf, (size_t)pid_len ) < (ssize_t)pid_len )
		{
			esmelog->lprint( "[-] Cannot write pid file \"%s\"", this->pid_fname );
			warn( "write() failed" );
			break;
		}
		
		ret = true;
	} while ( false );
	
	delete[] pid_buf;
	
	if ( !ret )
		exit( IESME_ERROR_FAILED );
	
	return( 0 );
}

void
iesmed::proceed_args( int argc, char ** argv )
{
	int opt;
	
	while ( true )
	{
		int opt_index = 0;
		opt = getopt_long( argc, argv, "i:df", arg_options, &opt_index );
		if ( opt == -1 )
			break;
		
		switch( opt )
		{
			case 'i':
				if ( optarg )
				{
					esmeid = atoi(optarg);
				}
				else
				{
					warn( "Wrong -i or --id argument!" );
					exit(0);
				}
				break;

			case 'd':
				DEBUG_LOG( "proceed_args(): debug is on\n" )
				this->debug = true;
				break;
				
			case 'f':
				DEBUG_LOG( "proceed_args(): foreground is on\n" )
				this->background = false;
				break;
		}
	}
}

bool inline
iesmed::fill_smscopts( pgrow_t& sql_row, smscopts_t * s_smscopts )
{
	bool loc_ok = false;
	
	do
	{
		if ( (s_smscopts->id = atoi( sql_row["id"].c_str() )) <= 0 )
		{
			esmelog->lprint( "[-] Null-id smsc read from DB!" );
			break;
		}
		
		strncpy( s_smscopts->address, sql_row["ip"].c_str(), SMSC_IP_MAX_LEN - 1 ); // 1.1.1.1
		if ( strlen( s_smscopts->address ) < 7 )
		{
			esmelog->lprint( "[-] There seems to be an error - length of address \"%s\" for SMSC #%d is fewer than 7 chars!", s_smscopts->address, s_smscopts->id );
			break;
		}
		
		s_smscopts->port = atoi( sql_row["port"].c_str() );
		if ( s_smscopts->port <= 0 || s_smscopts->port > 65535 )
		{
			esmelog->lprint( "[-] Invalid port \"%d\" for SMSC #%d!", s_smscopts->port, s_smscopts->id );
			break;
		}
		
		;
		if ( (s_smscopts->bind_type = atoi( sql_row["bind_type"].c_str() )) > 2 )
		{
			esmelog->lprint( "[-] Invalid bind_type \"%d\" for SMSC #%d!", s_smscopts->bind_type, s_smscopts->id );
			break;
		}
		
		s_smscopts->interface_version = atoi( sql_row["int_ver"].c_str() );
		if ( s_smscopts->interface_version <= 0
			|| s_smscopts->interface_version != 0x34 ) // NOTICE if some new proto versions will be added
		{
			esmelog->lprint( "[-] Invalid protocol_version \"%d\" for SMSC #%d!", s_smscopts->interface_version, s_smscopts->id );
			break;
		}
		
		s_smscopts->addr_ton = atoi(		sql_row["addr_ton"].c_str());
		s_smscopts->addr_npi = atoi(		sql_row["addr_npi"].c_str());
		s_smscopts->enquire_time = atol(	sql_row["enquire_time"].c_str());
		s_smscopts->throttling_timeout = atol(	sql_row["throttling_timeout"].c_str());
		s_smscopts->rebind_timeout = atol(	sql_row["rebind_timeout"].c_str() );

		strncpy( s_smscopts->system_id,		sql_row["system_id"].c_str(),	SMSC_SYSTEM_ID_MAX_LEN - 1 );
		strncpy( s_smscopts->password,		sql_row["passwd"].c_str(),	SMSC_PASSWORD_MAX_LEN - 1 );
		strncpy( s_smscopts->address_range,	sql_row["addr_rang"].c_str(),	SMSC_ADDR_RANG_MAX_LEN - 1 );
		strncpy( s_smscopts->system_type,	sql_row["sys_type"].c_str(),	SMSC_SYSTYPE_MAX_LEN - 1 );
		
		loc_ok = true;
	} while( 0 );
	
	esmelog->lprint( "[+] Read SMSC:\n"
			 "id = %d\naddress = %s; port = %d\n"
			 "bind_type = %d; interface_version = %d\n"
			 "system_id = %s; password = %s; system_type = %s\n"
			 "addr_ton = %d; addr_npi = %d; addres_range = %s\n"
			 "enquier_time = %ld; throttling_timeout = %ld; rebind_timeout = %ld", \
				s_smscopts->id, \
				s_smscopts->address, \
				s_smscopts->port, \
				s_smscopts->bind_type, \
				s_smscopts->interface_version, \
				s_smscopts->system_id, \
				s_smscopts->password, \
				s_smscopts->system_type, \
				s_smscopts->addr_ton, \
				s_smscopts->addr_npi, \
				s_smscopts->address_range, \
				s_smscopts->enquire_time, \
				s_smscopts->throttling_timeout, \
				s_smscopts->rebind_timeout \
			);
	
	return( loc_ok );
}

void
iesmed::timer()
{
	if ( !smscs_count || !timer_enabled )
		return;

	for ( size_t i = 0; i < smscs_count; ++i )
	{
		switch( smscs[i]->get_state() )
		{
			case SMSC_STATE_CLOSED:
			case SMSC_STATE_CLOSED_BY_ERR:
			case SMSC_STATE_CONNECTED_WAIT_BIND_RESP:
			case SMSC_STATE_BOUND_TX:
			case SMSC_STATE_BOUND_RX:
			case SMSC_STATE_BOUND_TXRX:
			case SMSC_STATE_WAIT_UNBIND_RESP:
			case SMSC_STATE_WAIT_OUTBIND:
				smscs[i]->timer();
				break;
		}
	}
}

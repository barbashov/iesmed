#include "socket_client.h"

socket_client::socket_client()
	: sock_d( -1 ),
	  s_connected( false ),
	  dummy_buffer( NULL ),
	  dummy_buffer_len( 32 )
{
	m_read = new mutex();
	m_write = new mutex();
	
	dummy_buffer = (char *)malloc( sizeof(char) * dummy_buffer_len );
}

socket_client::~socket_client( void )
{
	disconn();
	
	//printf( "~socket_client: free()\n" );
	free( dummy_buffer );
	//printf( "del m_read; " );
	delete m_read;
	//printf( "del m_write\n" );
	delete m_write;
	
	//printf( "~socket_client done\n" );
}

int
socket_client::conn( const char * sock_ip, unsigned int port )
{
	if ( s_connected )
		disconn();
	
	m_read->lock();
	m_write->lock();
	
	struct sockaddr_in s_client;
	bool err = true;

//	bzero( (char*)&s_client, sizeof(struct sockaddr_in) );
	s_client.sin_family = AF_INET;
	s_client.sin_port = htons( port );
	s_client.sin_addr.s_addr = inet_addr( sock_ip );
	
	do
	{
		if ( (sock_d = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP )) < 0 )
			break;

		if ( connect( sock_d, (struct sockaddr *)&s_client, sizeof( struct sockaddr ) ) < 0 )
		{
			perror("socket_client::conn()");
			break;
		}

/*		int keepalive = 0;
		if ( setsockopt(sock_d, SOL_SOCKET, SO_KEEPALIVE, (void*)&keepalive, sizeof(keepalive)) == -1 )
			break; */

		if ( fcntl(sock_d, F_SETFL, O_NONBLOCK | fcntl(sock_d, F_GETFL, 0)) == -1 )
			break;
		
		err = false;
	} while ( 0 );
	
	s_connected = !err;
	
	m_write->unlock();
	m_read->unlock();

	
	if ( err ) {
		return( IESME_ERROR_FAILED );
	}
	return( IESME_ERROR_OK );
}

bool
socket_client::connected()
{
	return(s_connected);
}

ssize_t
socket_client::rcv( void * buffer, size_t buf_len )
{
	if ( !sock_d || !s_connected )
		return IESME_ERROR_FAILED;
	
	ssize_t r_len;
	
	m_read->lock();
	while ( true ) {
		r_len = recv( sock_d, buffer, buf_len, 0 );
		
		if ( r_len < 1 ) {
			if ( r_len == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) )
				continue;
				
			disconn();
			m_read->unlock();
			return( -1 );
		}
		else {
			m_read->unlock();
			if ( s_length_enabled )
				total_length += r_len;
			return( r_len );
		}
	}
}

ssize_t
socket_client::rcv( char * buffer, size_t buf_len )
{
	return( rcv( (void *)buffer, buf_len ) );
}

/*ssize_t
socket_client::readn( char * buffer, size_t len )
{
	if ( !sock_d || !s_connected )
		return IESME_ERROR_FAILED;
	
	size_t r_len = len;
	ssize_t stat;
	
	while ( r_len > 0 )
	{
		stat = rcv( buffer, r_len );
		if ( stat < 0 )
		{
			if ( errno == EINTR )
				continue;
				
			return( errno );
		}
		else if ( stat == 0 )
		{
			return ( len - r_len );
		}
		buffer += stat;
		r_len -= stat;
	}
	
	return( len );
}*/

ssize_t
socket_client::snd( const char * buffer, size_t len )
{
	//printf( "socket_client::snd()\n" );

	if ( !sock_d || !s_connected )
		return IESME_ERROR_FAILED;
	
	size_t s_len = len;
	ssize_t stat;
	
	m_write->lock();
	while ( s_len > 0 )
	{
		stat = send( sock_d, static_cast<const void *>(buffer), s_len, 0 );
		if ( stat == -1 )
		{
			if ( errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK )
				continue;
				
			disconn();
			m_write->unlock();
			return( -1 );
		}
		s_len -= stat;
		buffer += stat;
	}
	
	m_write->unlock();
	return( len );
}

ssize_t
socket_client::snd( const void * buffer, size_t len )
{
	return( snd( static_cast<const char *>( buffer ), len ) );
}

int
socket_client::disconn()
{
	bool err = true;
	
	//printf( "m_read->lock()\n" );
	m_read->lock();
	//printf( "m_write->lock()\n" );
	m_write->lock();
	
	//printf( "disconnecting()\n" );
	
	do
	{
		if ( !sock_d || !s_connected )
			break;
	
		//printf( "shutdown()\n" );
		if ( !shutdown( sock_d, SHUT_RDWR ) )
			break;
		
		//printf( "close()\n" );
		if ( close( sock_d ) )
			break;
		
		err = false;
	}
	while ( 0 );
	
	sock_d = -1;
	s_connected = false;
	
	//printf( "unlocks()\n" );
	m_write->unlock();
	m_read->unlock();
	//printf( "unlocked()\n" );
	
	if ( err )
		return( IESME_ERROR_FAILED );
	return( IESME_ERROR_OK );
}


int
socket_client::wait_income( time_t _timeout )
{
	if ( !sock_d || !s_connected )
		return IESME_ERROR_FAILED;
	
	timeout.tv_sec = _timeout;
	timeout.tv_usec = 0;
	
	FD_ZERO( &sock_set );
	FD_SET( sock_d, &sock_set );
	
	return( select( sock_d + 1, &sock_set, NULL, NULL, &timeout ) > 0 );
}

bool
socket_client::true_income()
{
	return( FD_ISSET(sock_d, &sock_set) );
}

uint8
socket_client::r8()
{
	char read;
	if ( rcv( (void *)&read, sizeof( uint8 ) ) < 1 )
		return 0;
	
	uint8 ret = *((uint8 *)&read);
	
	return ( ret );
}

uint16
socket_client::r16h()
{
	char read[2];
	uint16 ret;
	if ( rcv( (void *)&read, sizeof( uint16 ) ) < 1 )
		return 0;
	
	ret = *((uint16 *)read);
	
	//printf( "r16h: %02X; %02X", ret, ntohs(ret) );
	return ntohs(ret);
}

uint32
socket_client::r32h()
{
	char read[4];
	uint32 ret, tmp;
	
	if ( rcv( (void *)&read, sizeof( uint32 ) ) < 1 )
		return 0;
	
	ret = *((u_int32_t *)read);
	u_int32_t reth = ntohl(ret);
	//printf( "r32h: %d; %d;\n", ret, reth );
	//printf( "r32h: %08X; %08X;\n", ret, reth );
	
	return reth;
}

char
socket_client::rc()
{
	char ret;
	if ( rcv( (void *)&ret, sizeof( char ) ) < 1 )
		return 0;
		
	return ret;
}

ssize_t
socket_client::rs( char * buf )
{
	char curr;
	int res;
	size_t len = 0;
	buf[0] = 0x00;
	
	do
	{
		if ( (res = rcv( &curr, sizeof( char ) )) < 1 )
			return( res );
		
		//printf( "segfault?" );
		buf[len++] = curr;
	}
	while ( curr != 0x00 );
	
	return( len );
}

ssize_t
socket_client::rcs( char * buf, size_t len )
{
	size_t r_len = rcv( buf, len );
//	if ( buf[r_len] != 0x00 )
	buf[r_len] = 0x00;
	
	return( r_len );
}

int
socket_client::dummy_read( size_t len )
{
	if ( !len )
		return 0;
	if ( !sock_d || !s_connected )
		return -1; 
	
	ssize_t r_len;
	ssize_t ret = 0;
	char temp;
	
/*	if ( len > dummy_buffer_len )
	{
		dummy_buffer_len *= 2;
		dummy_buffer = (char*)realloc( (void*)dummy_buffer, dummy_buffer_len );
	} */
	
	m_read->lock();
	while ( true )
	{
		r_len = recv( sock_d, &temp, 1, 0 );
		if ( r_len < 0 )
		{
			if ( errno == EINTR )
				continue;
				
			m_read->unlock();
			return -1;
		}
		else {
			ret += r_len;
			len -= r_len;
		}
		
		if (!r_len || !len) {
			m_read->unlock();
			if ( s_length_enabled )
				total_length += ret;
			return ret;
		}
	}
}

void
socket_client::length_toggle( bool _length_enabled )
{
	s_length_enabled = _length_enabled;
}

bool
socket_client::length_enabled()
{
	return( s_length_enabled );
}

void
socket_client::length_reset()
{
	total_length = 0;
}

ssize_t
socket_client::length_get()
{
	return( total_length );
}

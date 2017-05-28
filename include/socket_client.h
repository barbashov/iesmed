/*
 * 12 Dec 2006
 * Barbashov Ilya for Info-Tel
 * barbashov.ilya@gmail.com
 */

#ifndef __SOCKET_CLIENT_H_
#define __SOCKET_CLIENT_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "errors.h"
#include "mutex.h"
#include "types.h"

class socket_client
{
public:
	socket_client();
	~socket_client();
	
	int conn( const char * sock_ip, unsigned int port );
	
	ssize_t rcv( void * buffer, size_t buf_len );
	ssize_t rcv( char * buffer, size_t buf_len );
//	ssize_t readn( char * buffer, size_t buf_len );
	
	ssize_t snd( const char * buffer, size_t len );
	ssize_t snd( const void * buffer, size_t len );
	
	int disconn();
	bool connected();
	
	int wait_income( time_t );
	bool true_income();
	
	/*
	 * Functions for recieved length counting
	 */
	bool length_enabled();
	void length_toggle( bool );
	void length_reset();
	ssize_t length_get();
	
public:
	/*
	 * Some functions to read various types of data
	 */
	uint8 r8();	// unsigned char
//	uint16 r16();	// unsigned short int
//	uint32 r32();	// unsigned int
	
	// --//--//-- with network-to-host conversion
//	uint8 r8h();	// just pseudoname to r8() - one byte, no conversion
	uint16 r16h();
	uint32 r32h();
	
	char rc();			// char
	ssize_t rs( char * );		// string (reads while stream won't contain zero-symbol)
	ssize_t rcs( char *, size_t );  // string, but ignores zeros
	
	int dummy_read( size_t );
	
private:
	int sock_d;
	bool s_connected;
	
	mutex * m_read;
	mutex * m_write;
	
	struct timeval timeout;
	fd_set sock_set;
	
	bool s_length_enabled;
	ssize_t total_length;
	
	char * dummy_buffer;
	size_t dummy_buffer_len;
};


#endif

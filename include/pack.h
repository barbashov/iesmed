/*
 * 09 Feb 2007
 * Barbashov Ilya for Info-Tel
 * barbashov.ilya@gmail.com
 */

#ifndef __PACK_H__
#define __PACK_H__

#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#include "mutex.h"

#define DEFAULT_PACK_LEN	256

class pack
{
public:
	pack()
		: pack_real_len( DEFAULT_PACK_LEN ),
		  pack_len( 0 )
	{
		m_pack = new mutex();
		c_pack = (char *)malloc( pack_real_len );
	}
	~pack()
	{
		free( c_pack );
	
		delete m_pack;
	}
	
	void clear()
	{
		m_pack->lock();
		pack_len = 0;
		m_pack->unlock();
	}
	
	// next tree functions write unsigned integer in packet with size accordingly at function name
	void w8( const uint8 val )
	{
		//printf( "w8: %d\n", val );
		c_pack[pack_len++] = val;
	}
	void w16( const uint16 val )
	{
		u16 = val;
		for ( uint8 i = 0; i < 2; ++i )
		{
			c_pack[pack_len++] = ( *(((char *)(&u16)) + i) );
		}
	}
	void w32( const uint32 val )
	{
		u32 = val;
//		pput( (char *)&u32, sizeof( uint32 ) );
		//printf( "w32n: (should be 0x%08X) wrote ", val );
		for ( uint8 i = 0; i < 4; ++i )
		{
			c_pack[pack_len++] = ( *(((char *)(&u32)) + i) );
			//printf( "%02X ", c_pack[pack_len - 1] & 0xFF );
		}
		//printf( "\n" );
	}
	void w16n( const uint16 val )
	{
		w16( htons(val) );
	}
	void w32n( const uint32 val )
	{
		w32( htonl(val) );
	}
	
	// writes a string to packet with zero-symbol at the end
	void ws( const char * val )
	{
		size_t len = strlen( val ) + 1;
		
		m_pack->lock();
		char * dest = c_pack + pack_len;
		
		if ( pack_len + len > pack_real_len )
			resize( len );
			
		strcpy( dest, val );
		pack_len += len;
		
		//printf( "ws: c_pack now: " );
		for ( int i = 0; i < pack_len; i++ )
		{
			//printf( "%02X ", c_pack[i] );
		}
		//printf( "\n" );
		
		m_pack->unlock();
	}
// writes a C-octet string without zero-symbol at the end
	void wcs( const char * val )
	{
		m_pack->lock();
		
		this->ws( val );
		pack_len--;
		
		m_pack->unlock();
	}
	// "low-level" packet put :)
	void pput( const char * src, size_t len )
	{
		size_t i;
		
		if ( pack_len + len + 1 > pack_real_len )
			resize( len );
		
		for ( i = 0; i < len; ++i )
		{
			c_pack[pack_len++] = src[i] & 0xFF;
		}
		
		//printf( "pput(char): c_pack now: " );
		for ( i = 0; i < pack_len; ++i )
		{
			//printf( "%02X ", c_pack[i] );
		}
		//printf( "\n" );
	}
	
	ssize_t out( char * buffer )
	{
		//printf( "out: " );
		for ( int i = 0; i < pack_len; i++ )
		{
			//printf( "%02X ", c_pack[i] );
		}
		//printf( "\n" );
		
		memcpy( (void *)(buffer), (void *)(c_pack), pack_len );
		return( pack_len );
	}
	size_t size()
	{
		return( pack_len );
	}
	
private:
	// resizes a packet if it need
	void resize( int req_len = 4 )
	{
		m_pack->lock();
		// I made it so 'cause it's better way. Default value of req_len at the top too.
		while ( pack_real_len - pack_len - req_len < 0 )
			pack_real_len *= 2;
			
		c_pack = (char *)realloc( (void *)c_pack, pack_real_len );
		m_pack->unlock();		
	}
	void pput( void * src, size_t len )
	{
		m_pack->lock();
		
		if ( pack_len + len > pack_real_len )
			resize( len );
			
		//printf( "putting %p to pack\n", src );
		memcpy( (void *)(c_pack + pack_len), &src, len );
//		printf( "pack now: %p\n", (void *)c_pack );
		pack_len += len;
		
		//printf( "\n" );
		for ( int i = 0; i < pack_len; i++ )
		{
			//printf( "%02X ", c_pack[i] );
		}
		//printf( "\n" );
		
		m_pack->unlock();
	}
	
private:
	char * c_pack;
	size_t pack_real_len;
	size_t pack_len;
	
	uint32 u32;
	uint16 u16;
	
	mutex * m_pack;
};

#endif

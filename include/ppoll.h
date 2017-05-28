/*
 * 07 Feb 2007
 * Barbashov Ilya for Info-Tel
 * barbashov.ilya@gmail.com
 */

#ifndef __PPOLL_H__
#define __PPOLL_H__

#include <stdlib.h>
#include <string.h>

#include "mutex.h"

#define PPOLL_DEFAULT_SIZE	2

#define PRIORITY_LOW		-1
#define PRIORITY_NORMAL		 0
#define PRIORITY_HIGH		 1

template < typename T >
class ppoll
{
private:
	typedef struct _ppoll_elem_t
	{
		T data;
		char priority;
	} ppoll_elem_t;

public:
	ppoll()
		: real_size( PPOLL_DEFAULT_SIZE ),
		  ppoll_max( 0 ),
		  ppoll_data_size( 0 )
	{
		m_poll = new mutex();
		ppoll_data = (ppoll_elem_t *)malloc( sizeof( ppoll_elem_t ) * real_size );
	}
	~ppoll()
	{
		free( ppoll_data );
		
		delete m_poll;
	}

	void push( const T& _data, char _priority )
	{
		m_poll->lock();
	
		if ( real_size + 1 < ppoll_data_size )
			resize();
			
		ppoll_data[ppoll_data_size].data = _data;
		ppoll_data[ppoll_data_size].priority = _priority;
		
		if ( ppoll_data[ppoll_max].priority < _priority )
			ppoll_max = ppoll_data_size;
			
		++ppoll_data_size;
		
		m_poll->unlock();
	}
	void pop()
	{
		if ( !ppoll_data_size )
			return;
		
		m_poll->lock();
		
		memcpy( \
			static_cast<void *>(&ppoll_data[ppoll_max]), \
			static_cast<const void *>(&ppoll_data[ppoll_max + 1]), \
			sizeof( ppoll_elem_t ) * (ppoll_data_size - ppoll_max - 1) \
		);
		
		--ppoll_data_size;
		
		max();
		
		m_poll->unlock();
	}
	
	void clear()
	{
		m_poll->lock();
		ppoll_data_size = 0;
		m_poll->unlock();
	}
	
	T * top()
	{
		if ( !ppoll_data_size )
			return( NULL );
		return( &ppoll_data[ppoll_max].data );
	}
	size_t size()
	{
		return( ppoll_data_size );
	}
	
private:
	void resize()
	{
		m_poll->lock();
		
		real_size *= 2;
		ppoll_data = (ppoll_elem_t *)realloc( static_cast<void *>(ppoll_data), sizeof( ppoll_elem_t ) * real_size );
		
		m_poll->unlock();
	}
	
	void max()
	{
		m_poll->lock();
		if ( ppoll_data_size == 0 )
		{
			ppoll_max = 0;
			m_poll->unlock();
			return;
		}
		
		ppoll_max = 0;
		for ( size_t i = 1; i < ppoll_data_size; ++i )
		{
			if ( ppoll_data[ppoll_max].priority < ppoll_data[i].priority )
				ppoll_max = i;
		}
		m_poll->unlock();
	}
	
private:
	ppoll_elem_t * ppoll_data;
	
	size_t ppoll_max;
	size_t ppoll_data_size;
	size_t real_size;
	
	mutex * m_poll;
};

#endif // __PPOLL_H__

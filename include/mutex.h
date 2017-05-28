/*
 * 08 Feb 2007
 * Barbashov Ilya for Info-Tel
 * barbashov.ilya@gmail.com
 */

#ifndef __MUTEX_H__
#define __MUTEX_H__

#include <pthread.h>

class mutex
{
public:
	mutex()
		: locks( 0 )
	{
		pthread_mutex_init( &mutex_o, NULL );
	}
	~mutex()
	{
		pthread_mutex_unlock( &mutex_o );
		pthread_mutex_destroy( &mutex_o );
	}
	
	void lock()
	{
//		printf( "locking mutex 0x%08X for thread 0x%08X\n", this, pthread_self() );
		if ( pthread_equal( pthread_self(), mutex_thread ) )
		{
			locks++;
			return;
		}
		
//		printf( "locking mutex\n" );
		pthread_mutex_lock( &mutex_o );
//		printf( "setting mutex_thread\n" );
		mutex_thread = pthread_self();
		locked = true;
	}
	void unlock()
	{
//		printf( "unlocking mutex 0x%08X for thread 0x%08X\n", this, mutex_thread );
		if ( !pthread_equal( pthread_self(), mutex_thread ) || !locked )
			return;
		
		if ( locks > 0 )
		{
			locks--;
			return;
		}
		
		locked = false;
		mutex_thread = 0;
		
		pthread_mutex_unlock( &mutex_o );
	}
	
	pthread_t thread()
	{
		return( mutex_thread );
	}
	
private:
	pthread_t mutex_thread;
	pthread_mutex_t mutex_o;
	
	size_t locks;
	bool locked;
};

#endif // __MUTEX_H__

/*
 * 19 Nov 2006 - Nowadays
 * Barbashov Ilya for Info-Tel
 * 
 * iESME daemon
 *
 * All questions to: barbashov.ilya@gmail.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>

#include "errors.h"
#include "constants.h"
#include "globals.h"
#include "log.h"
#include "postgres.h"
#include "iesmed.h"

bool iesmed_created = false;
static iesmed* iesme = NULL;

int alrms = 0;

void
cb_sigterm( int sig )
{
	DEBUG_LOG( "Got SIGTERM or SIGINT. Abort.\n" );
//	last_signal_code = SIGTERM;
	if ( iesme )
	{
		delete iesme;
		iesme = NULL;
	}
	exit( IESME_ERROR_OK );
}

void
cb_sigalrm( int sig )
{
//	printf( "Got SIGALRM %d\n", ++alrms );
//	last_signal_code = SIGALRM;
	iesme->timer();
}

int 
main( int argc, char ** argv )
{
	iesmed_created = false;
	
	if ( signal( SIGTERM, cb_sigterm ) == SIG_ERR )
		errx( IESME_ERROR_FAILED, "SIGTERM callback assignment failed" );
	
	if ( signal( SIGALRM, cb_sigalrm ) == SIG_ERR )
		errx( IESME_ERROR_FAILED, "SIGALRM callback assignment failed" );

	if ( signal( SIGINT, cb_sigterm ) == SIG_ERR )
		errx( IESME_ERROR_FAILED, "SIGINT callback assignment failed" );
	
	DEBUG_LOG( "creating iesmed\n" );
	
	iesme = new iesmed( argc, argv );
	
	struct itimerval itimer = { 1, 0, 1, 0 };
	setitimer( ITIMER_REAL, &itimer, (struct itimerval *)NULL );
	
	if ( iesme->init_failed )
	{
		errx( IESME_ERROR_FAILED, "daemon init failed" );
	}
	else
	{
		if ( iesme->start() != 0 )
		{
			errx( IESME_ERROR_FAILED, "daemon start failed" );
		}
		DEBUG_LOG( "iesme->start() exited\n" );
	}
	
	if ( iesme )
		delete iesme;
	
	return IESME_ERROR_OK;
}

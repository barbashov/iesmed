/*
 * 24 Nov 2006
 * Barbashov Ilya for Info-Tel
 * barbashov.ilya@gmail.com
 */

#ifndef __IESMED_H_
#define __IESMED_H_

#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>
#include <getopt.h>
#include <string.h>
#include <vector>

#include "types.h"
#include "errors.h"
#include "globals.h"
#include "log.h"
#include "postgres.h"
#include "smsc.h"

#define PARENT_USLEEP 1000
#define TLV_MAP_DEFAULT 5000

class iesmed
{
public:
	iesmed( int argc, char ** argv );
	~iesmed();
	
	int start();
	void timer();
	
	bool init_failed;
	
private:
	void proceed_args( int argc, char ** argv );
	void do_fork();
	int write_pidfile();
	bool inline fill_smscopts( pgrow_t&, smscopts_t * );
	
private:
	int db_read_smscs();
	int db_read_tlv_map();
	
	log_t * esmelog;
	postgres * db;
	
	uint8 esmeid;
	bool background;
	bool debug;
	
	int pid;
	char* pid_fname;
	int pid_fd;
	
	bool forked;
	
	bool iesmed_alive;
	
	smsc ** smscs;
	size_t smscs_count;
	bool timer_enabled;
	
	tlv_map_t * g_tlv_map;
	uint16 tlv_max_tag;
};

#endif

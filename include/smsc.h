/*
 * 01 Dec 2006
 * Barbashov Ilya for Info-Tel
 * barbashov.ilya@gmail.com
 *
 * Камень преткновения, блин :)
 */

/*
 * ATTENTION!!!
 *
 * Table tlv_map arch goes here:
 * CREATE TABLE tlv_map (
 *	id serial,
 *	tag int UNIQUE NOT NULL,
 *	value_type smallint NOT NULL DEFAULT 0,
 *	column_name text UNIQUE,
 *	store_as smallint NOT NULL DEFAULT 0,
 *	description text
 * );
 *
 * value_type can be:
 * 0 - no value
 * 1 - unsigned char
 * 2 - unsigned short int
 * 3 - unsigned int
 * 4 - string
 *
 * store_as can be:
 * 0 - no value
 * 1 - don't store
 * 2 - store as integer
 * 3 - store as string
 *
 * column_name max length - 127 symbols
 * description max length - 255 symbols
 *
 * NOTICE All other values of value_type and store_as will thrown by ESME.
 * 	  And you must remember, that tlv_set is used by ESME not only for incoming PDUs, 
 *  but also for forming some outcoming.
 */

#ifndef __SMSC_H_
#define __SMSC_H_

#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <iconv.h>

#include "types.h"
#include "socket_client.h"
#include "constants.h"
#include "log.h"
#include "mutex.h"
#include "errors.h"
#include "pack.h"
#include "ppoll.h"
#include "postgres.h"
#include "smpp.h"
#include "segments.h"


#define BIND_TYPE_TX				0	// Transmitter - ESME only sends submit_sm
#define BIND_TYPE_RX				1	// Reciever - ESME only get deliver_sm
#define BIND_TYPE_TXRX				2	// Transciever - ESME both sends submit_sm and gets deliver_sm in one full-duplex socket

// Константы для структуры smsc_options
#define SMSC_IP_MAX_LEN				19
#define SMSC_SYSTEM_ID_MAX_LEN			17
#define SMSC_PASSWORD_MAX_LEN			10
#define SMSC_ADDR_RANG_MAX_LEN			42
#define SMSC_SYSTYPE_MAX_LEN			14

// Константы состояний линка с СМСЦ
#define SMSC_STATE_CLOSED			1
#define SMSC_STATE_CLOSED_NEED_CONNECT		2
#define SMSC_STATE_CLOSED_BY_ERR		3
#define SMSC_STATE_CONNECTED			4
#define SMSC_STATE_CONNECTED_WAIT_BIND_RESP	5
#define SMSC_STATE_BOUND_TX			6
#define SMSC_STATE_BOUND_RX			7
#define SMSC_STATE_BOUND_TXRX			8
#define SMSC_STATE_UNBIND_BY_SMSC		9
#define SMSC_STATE_UNBIND_BY_ESME		10
#define SMSC_STATE_WAIT_UNBIND_RESP		11
#define SMSC_STATE_WAIT_OUTBIND			12
#define SMSC_STATE_AWAITING_START		253
#define SMSC_STATE_AWAITING_DESTROY		254
#define SMSC_STATE_DESTROYED			255

static char * error_desc[ECODES_COUNT];
static char * bind_string[3];
static char * value_types_args[4];

#define LISTENER_TIMEOUT			60
#define OUT_SEQ_NUMBER_MAX			2147483600
#define DELIVER_SM_INSERT_LEN			65535 * 2
#define LOG_VAL_DEFAULT_LENGTH			32
#define PDU_BODY_DEFAULT_LENGTH			sizeof(char) * 256

#define TIMEOUTS_COUNT				3
#define TIMEOUT_SELECT				0
#define TIMEOUT_BIND				1
#define TIMEOUT_SUBMIT_SM			2

#define ENCODING_UCS				"UCS-2BE"
#define ENCODING_KOI8				"KOI8-R"

#define IN_MESSAGE_DEFAULT_LEN			256
#define OUT_MESSAGE_DEFAULT_LEN			512

#define DELIVER_SM_INSERT_TEMPLATE		"INSERT INTO deliver_sm (esmeid, smscid, serv_type, srcaddr_ton, srcaddr_npi, src_addr, dstaddr_ton, dstaddr_npi, dst_addr, esm_class, protocol_id, priority_flag, data_coding, mess%s)" \
						" VALUES ( '%d', '%d', '%s', '%d', '%d', '%s', '%d', '%d', '%s', '%d', '%d', '%d', '%d', '%s'%s)"

// 69 - FIXME
#define MESS_ATOM_LENGTH(__data_coding)	((__data_coding == DC_UCS2BE || __data_coding == DC_UNI) ? 69 : 170)

typedef struct _smscopts_t
{
	int id;
	char address[SMSC_IP_MAX_LEN];
	int port;
	char system_id[SMSC_SYSTEM_ID_MAX_LEN];
	char password[SMSC_PASSWORD_MAX_LEN];
	uint8 bind_type;
	uint8 interface_version;
	
	uint8 addr_ton;
	uint8 addr_npi;
	char address_range[SMSC_ADDR_RANG_MAX_LEN];
	char system_type[SMSC_SYSTYPE_MAX_LEN];
	uint32 enquire_time;
	uint32 throttling_timeout;
	uint32 rebind_timeout;
} smscopts_t;

typedef struct _queue_elem_t
{
	char * data;
	size_t data_len;
} queue_elem_t;


class smsc
{
public:
	smsc( smscopts_t *, log_t *, uint8, tlv_map_t *, size_t );
	~smsc();
	
	int start();
	int stop();
	int smsc_id();
	
	void set_state( uint8 new_state );
	uint8 get_state();
	
	int listener();
	int writer();
	int db_listener();
	
	void timer();
	int conn();
	
private:
	static void * _listener( void * );
	static void * _writer( void * );
	static void * _db_listener( void * );
	
	void proceed_pdu();
	void make_pdu_header( char *, uint32, uint32, uint32, uint32 );
	void make_bind_pdu();

	int message_decode( char *, size_t, char *, uint8 );
	int message_encode( char *, size_t, char *, uint8 );
	
	void insert_pdu_in_out_queue( char *, char *, size_t, char );
	int proceed_tlvs();
	void free_tlvs_dynarg();
	
	void disconn( uint8 _new_state );
	
	void resize_pdu_body( size_t );
	
	void set_timeout( uint8, uint32 );
	uint32 get_timeout( uint8 );

	uint32 roll_seqnum();
	
public:
	bool listener_alive;
	bool writer_alive;
	bool db_listener_alive;
	
private:
	smscopts_t * options;
	uint8 smsc_state;
	mutex * m_state;
	socket_client * sock;
	uint8 disconnects_number;
	
	ppoll< queue_elem_t > out_queue;

	pthread_t t_listener;
	pthread_t t_writer;
	pthread_t t_db_listener;

	tlv_map_t * g_tlv_map;
	size_t tlvs_max_tag;

	log_t * esmelog;
	uint8 esmeid;
	
	postgres * db;
	postgres * db_tlistener;
	postgres * db_twriter;
	postgres * db_listen;
	
	time_t t_select;
	
	pack * o_pack;
	mutex * m_opack;
	pack * s_pack;
	
	size_t out_sequence_number;
	mutex * m_outseqnum;
	
	char * sql_deliver_sm_insert;
	char * sql_insert_additional_args;
	char * sql_insert_additional_vals;
	
	segments_t * segments;
	
	char * pdu_body;
	size_t pdu_body_length;
	char * pdu_header;
	bool pdu_header_exists;

	char * ssm_body;
	size_t ssm_body_size;
	char * ssm_header;

	char * writer_pdu_header;

	char * in_message;
	size_t in_message_len;
	char * out_message;
	size_t out_message_len;
	
	/*
	 * Temporary variables for incoming PDU
	 */
	
	uint32 pdu_command_length;
	uint32 pdu_command_id;
	uint32 pdu_command_status;
	uint32 pdu_sequence_number;
	
	char * log_val;
	size_t log_val_size;
	
	size_t tlvs_count;
	size_t tlvs_size;
	tlv_t * tlvs;
	
	time_t * timeouts;
	bool enquire_timeouted;
	bool was_submit_sm_timeout;
	bool db_listener_first_time;
	mutex * m_timeouts;

	iconv_t iv_encode;
	iconv_t iv_decode;
};

void smsc_init_constants();

#endif

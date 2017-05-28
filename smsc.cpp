#include "smsc.h"

smsc::smsc( smscopts_t * smsc_options, log_t * smsc_esmelog, uint8 smsc_esmeid, tlv_map_t* smsc_g_tlv_map, size_t smsc_tlvs_max_tag )
	: esmelog( smsc_esmelog ),
	  esmeid( smsc_esmeid ),
	  out_sequence_number(1),
	  pdu_body_length( PDU_BODY_DEFAULT_LENGTH ),
	  g_tlv_map(smsc_g_tlv_map),
	  was_submit_sm_timeout(false),
	  db_listener_first_time(true),
	  enquire_timeouted(false),
	  in_message_len(IN_MESSAGE_DEFAULT_LEN),
	  out_message_len(OUT_MESSAGE_DEFAULT_LEN),
	  iv_encode(NULL),
	  iv_decode(NULL),
	  tlvs_size(1),
	  tlvs_count(0),
	  ssm_body_size(256),
	  tlvs_max_tag(smsc_tlvs_max_tag),
          log_val_size(LOG_VAL_DEFAULT_LENGTH) 
{
	DEBUG_LOG( "smsc constructor called\n" );

	db = new postgres( DB_HOST, DB_NAME, DB_USER, DB_PASSWORD, this->esmelog );
	db_tlistener = new postgres( DB_HOST, DB_NAME, DB_USER, DB_PASSWORD, this->esmelog );
	db_twriter = new postgres( DB_HOST, DB_NAME, DB_USER, DB_PASSWORD, this->esmelog );
	
	options = new smscopts_t;
	memcpy( static_cast<void *>(options), static_cast<const void *>(smsc_options), sizeof( smscopts_t ) );	
	log_val = (char *)malloc( sizeof(char) * log_val_size + 1 );
	db_listen = new postgres( DB_HOST, DB_NAME, DB_USER, DB_PASSWORD, this->esmelog );
	o_pack = new pack();
	s_pack = new pack();
	pdu_body = (char *)malloc( pdu_body_length );
	
	sql_deliver_sm_insert = new char[DELIVER_SM_INSERT_LEN];
	sql_insert_additional_args = new char[DELIVER_SM_INSERT_LEN / 10];
	sql_insert_additional_vals = new char[DELIVER_SM_INSERT_LEN / 10];

	//printf( "smsc(): %s;%s\n", options->system_id, options->password );
	sock = new socket_client();
	m_state = new mutex();
	
	m_timeouts = new mutex();
	timeouts = (time_t *)malloc( sizeof(time_t) * TIMEOUTS_COUNT );
	for( size_t i = 0; i < TIMEOUTS_COUNT; ++i )
		set_timeout( i, 0 );
	
	set_timeout( TIMEOUT_SELECT, LISTENER_TIMEOUT );
	
	segments = new segments_t();

	writer_pdu_header = (char *)malloc( PDU_HEADER_LENGTH );
	pdu_header = (char *)malloc( PDU_HEADER_LENGTH );

	ssm_header = (char *)malloc( PDU_HEADER_LENGTH );
	ssm_body = (char *)malloc( ssm_body_size );

	in_message = (char *)malloc( sizeof(char) * in_message_len );
	out_message = (char *)malloc( sizeof(char) * out_message_len );

	m_outseqnum = new mutex();
	m_opack = new mutex();

	tlvs = (tlv_t *)malloc( sizeof(tlv_t) * tlvs_size );
	
	set_state( SMSC_STATE_AWAITING_START );
	
	DEBUG_LOG( "smsc constructor exited\n" );
}

smsc::~smsc()
{
	DEBUG_LOG( "stopping\n" );
	stop();
	
	set_state(SMSC_STATE_DESTROYED);
	delete m_state;

	delete options;
	delete db_listen;
	delete o_pack;
	delete s_pack;
	DEBUG_LOG( "deleting segments\n" );
	delete segments;
	delete sock;
	delete m_timeouts;
	delete m_outseqnum;
	delete m_opack;
	
	delete db;
	delete db_tlistener;
	delete db_twriter;

	delete[] sql_deliver_sm_insert;
	delete[] sql_insert_additional_args;
	delete[] sql_insert_additional_vals;

	free(ssm_body);
	free(ssm_header);

	free(log_val);
	free(pdu_body);
	free(timeouts);
	free(writer_pdu_header);
	free(pdu_header);
	free(in_message);
	free(out_message);

	if ( iv_encode )
		iconv_close( iv_encode );
	if ( iv_decode )
		iconv_close( iv_decode );
}

int 
smsc::start()
{
	DEBUG_LOG( "starting smsc\n" );

	if ( (iv_decode = iconv_open( ENCODING_KOI8, ENCODING_UCS )) == (iconv_t)(-1) )
	{
		esmelog->lprint( "[-] SMSC #%d: Error opening iconv for conversion from %s to %s.", smsc_id(), ENCODING_UCS, ENCODING_KOI8 );
		return IESME_ERROR_FAILED;
	}
	if ( (iv_encode = iconv_open( ENCODING_UCS, ENCODING_KOI8 )) == (iconv_t)(-1) )
	{
		esmelog->lprint( "[-] SMSC #%d: Error opening iconv for conversion from %s to %s.", smsc_id(), ENCODING_KOI8, ENCODING_UCS );
		return IESME_ERROR_FAILED;
	}

	sock->length_toggle( true );
	
	if ( db_listen->create_listener( OUTCOMING_NOTIFY ) < 0 )
	{
		esmelog->lprint( "[-] SMSC #%d: error while creating DB listener \"%s\"", smsc_id(), OUTCOMING_NOTIFY );
		return IESME_ERROR_FAILED;
	}
	DEBUG_LOG( "db_listener created\n" )
	

	listener_alive = true;
	if ( pthread_create( &t_listener, NULL, _listener, (void *)this ) )
	{
		esmelog->lprint( "[-] SMSC #%d: cannot start listener thread.", smsc_id() );
		return IESME_ERROR_FAILED;
	}
	DEBUG_LOG( "t_listener created\n" )
	
	writer_alive = true;
	if ( pthread_create( &t_writer, NULL, _writer, (void *)this ) )
	{
		esmelog->lprint( "[-] SMSC #%d: cannot start writer thread.", smsc_id() );
		return IESME_ERROR_FAILED;
	}
	DEBUG_LOG( "t_writer created\n" )
	
	db_listener_alive = true;
	if ( pthread_create( &t_db_listener, NULL, _db_listener, (void *)this ) )
	{
		esmelog->lprint( "[-] SMSC #%d: cannot start DB listener thread.", smsc_id() );
		return IESME_ERROR_FAILED;
	}
	DEBUG_LOG( "t_db_listener created\n" )
	
	set_state(SMSC_STATE_CLOSED_NEED_CONNECT);
	
	return IESME_ERROR_OK;
}

int
smsc::stop()
{
	if ( listener_alive )
	{
		listener_alive = false;
		DEBUG_LOG( "cancelling listener\n" );
		kill( t_listener, SIGTERM );
		pthread_cancel( t_listener );
	}
	if ( writer_alive )
	{
		writer_alive = false;
		DEBUG_LOG( "cancelling writer\n" );
		pthread_cancel( t_writer );
	}
	if ( db_listener_alive )
	{
		DEBUG_LOG( "cancelling db_listener\n" );
		db_listener_alive = false;
		db_listen->disconn();
	}
}

int
smsc::smsc_id()
{
	return( options->id );
}

int
smsc::conn()
{
	if ( get_timeout(TIMEOUT_BIND) )
		return IESME_ERROR_FAILED;

	esmelog->lprint( "[+] SMSC #%d: Connecting to SMSC.", smsc_id() );
	if ( sock->conn( options->address, options->port ) == -1 )
	{
		/*
		 * FIXME
		 *  Bad workaround
		 */
		if ( disconnects_number++ >= 5 ) {
			esmelog->lprint( "[-] SMSC #%d: Couldn't connect to SMSC (reason: %s). It's already fifth, killing myself :)", smsc_id(), strerror(errno) );
			kill( getpid(), 11 );
		}

		esmelog->lprint( "[-] SMSC #%d: Couldn't connect to SMSC (reason: %s). Will reconnect in %d seconds.", smsc_id(), strerror(errno), options->rebind_timeout );
		set_timeout( TIMEOUT_BIND, options->rebind_timeout );
		set_state(SMSC_STATE_CLOSED_BY_ERR);
		return IESME_ERROR_FAILED;
	}
	disconnects_number = 0;
	
	esmelog->lprint( "[+] SMSC #%d: Connected to SMSC.", smsc_id() );
	
	set_state(SMSC_STATE_CONNECTED);
	return IESME_ERROR_OK;
}

void
smsc::disconn( uint8 _new_state = 0 )
{
	// TODO implement fully
	
	sock->disconn();
	if ( _new_state )
		set_state(_new_state);
}

int
smsc::listener()
{
	if ( !sock->connected() )
	{
		esmelog->lprint( "[-] Unexpected disconnect. Going in CLOSED_BY_ERR state." );
		set_timeout( TIMEOUT_BIND, options->rebind_timeout );
		set_state(SMSC_STATE_CLOSED_BY_ERR);
		return IESME_ERROR_OK;
	}

	sock->length_reset();
	
//	t_select = time(NULL);
	DEBUG_LOG( "listener: select\n" );
	int res = sock->wait_income( get_timeout(TIMEOUT_SELECT) );
	DEBUG_LOG( "listener: select exited\n" );
//	printf( "listener: select exited in %d seconds\n", get_timeout(TIMEOUT_SELECT) );
	
	if ( res < 0 )
	{
		if ( errno == EINTR )
		{
			esmelog->lprint( "[-] SMSC #%d: It seems to be that select was interrupted by some signal. I'll continue.", smsc_id() );
			return( IESME_ERROR_OK );
		}

		esmelog->lprint( "[-] SMSC #%d: select: \"%s\". I'll continue.", smsc_id(), strerror( res ) );
		return( IESME_ERROR_OK );
	}
	else if ( sock->true_income() && res > 0 )
	{
		//printf( "incoming data, reading\n" );
		
/*		if ( (pdu_command_length = sock->r32h()) < 0 )
		{
			esmelog->lprint( "[-] SMSC #%d:recv() returned -1. Error #0x%08X: %s. Abort.", smsc_id(), errno, strerror(errno) );
			return IESME_ERROR_FAILED;
		} 
		else if (pdu_command_length < PDU_HEADER_LENGTH)
		{
			//printf( "something wrong - got PDU where command_length < %d\n", PDU_HEADER_LENGTH );
		}*/
		pdu_command_length = sock->r32h();
		pdu_command_id = sock->r32h();
		pdu_command_status = sock->r32h();
		pdu_sequence_number = sock->r32h();

		if ( !pdu_command_length || !pdu_command_id ) {
			esmelog->lprint( "[-] SMSC #%d: Some of PDU field is zero (except of command_status, of course). It's a serious error, I would set state to CLOSED_BY_ERR and reconnect in %d seconds.", smsc_id(), options->rebind_timeout );
//			sock->disconn();
//			set_timeout(TIMEOUT_BIND, options->rebind_timeout);
//			set_state(SMSC_STATE_CLOSED_BY_ERR);
			return IESME_ERROR_OK;
		}
		//printf( "read: %ld; 0x%08X; 0x%08X; %ld\n", pdu_command_length, pdu_command_id, pdu_command_status, pdu_sequence_number );
		
		DEBUG_LOG( "proceeding pdu\n" );
		proceed_pdu();
	}
	else
	{
		if (enquire_timeouted)
		{
			esmelog->lprint( "[-] SMSC #%d: No data from SMSC in %d seconds and no response after ENQUIRE_LINK. Seems to be that SMSC disconnected or pipe broke. Will try to recconect in %d seconds.", smsc_id(), get_timeout(TIMEOUT_SELECT), options->rebind_timeout );
			sock->disconn();
			set_timeout( TIMEOUT_BIND, options->rebind_timeout );
			set_state(SMSC_STATE_CLOSED_BY_ERR);
			DEBUG_LOG( "returning" );

			return( IESME_ERROR_OK );
		}

		esmelog->lprint( "[+] SMSC #%d: No data from SMSC in %d seconds. Sending ENQUIRE_LINK.", smsc_id(), get_timeout(TIMEOUT_SELECT) );

		make_pdu_header(pdu_header,
				PDU_HEADER_LENGTH,
				C_ENQUIRE_LINK,
				ESME_ROK,
			        roll_seqnum());

		insert_pdu_in_out_queue(pdu_header, NULL, 0, PRIORITY_HIGH);
		set_timeout(TIMEOUT_SELECT, options->enquire_time);
		
		enquire_timeouted = true;

		return IESME_ERROR_OK;
	}
	
	return( IESME_ERROR_OK );
}

void
smsc::proceed_pdu()
{
	pdu_header_exists = false;
	char * remote_system_id = (char *)malloc( 17 );
	char priority = PRIORITY_NORMAL;
	size_t body_length = 0;
	size_t i;
	int messages = 0;
	int result = -1;
	
	tlvs_count = 0;
	switch ( pdu_command_id )
	{
		case C_BIND_RECEIVER_RESP:
		case C_BIND_TRANSMITTER_RESP:
		case C_BIND_TRANSCEIVER_RESP:
			sock->rs( remote_system_id );
			esmelog->lprint( "[+] Got %s: length %d; command_status %d; sequence_number %d.", bind_string[pdu_command_id], pdu_command_length, pdu_command_status, pdu_sequence_number );
			if ( proceed_tlvs() == -1 )
				return;
			
			switch( pdu_command_status )
			{
				case ESME_ROK:
					if ( pdu_command_id == C_BIND_RECEIVER_RESP )
						set_state(SMSC_STATE_BOUND_RX);
					else if ( pdu_command_id == C_BIND_TRANSMITTER_RESP )
						set_state(SMSC_STATE_BOUND_TX);
					else
						set_state(SMSC_STATE_BOUND_TXRX);
					esmelog->lprint( "[+] SMSC #%d: Bound to %s", smsc_id(), remote_system_id );
					break;
					
				case ESME_RBINDFAIL:
					esmelog->lprint( "[-] SMSC #%d: SMSC said %s. Disconnecting. Will try reconnect.", smsc_id(), error_desc[pdu_command_status] );
					disconn(SMSC_STATE_CLOSED_BY_ERR);
					set_timeout( TIMEOUT_BIND, options->rebind_timeout );
					break;
					
				default:
					esmelog->lprint( "[-] SMSC #%d: SMSC said %s. Disconnecting and destroing.", smsc_id(), error_desc[pdu_command_status] );
					disconn( SMSC_STATE_AWAITING_DESTROY );
			}
			break;
		
		case C_UNBIND:
			set_timeout( TIMEOUT_BIND, options->rebind_timeout );
//			set_state(SMSC_STATE_UNBIND_BY_SMSC);
			set_state(SMSC_STATE_CLOSED_BY_ERR);
			esmelog->lprint( "[+] SMSC #%d: Got unbind. Reconnecting in %d seconds.", smsc_id(), options->rebind_timeout );
			break;
		case C_UNBIND_RESP:
			disconn( SMSC_STATE_CLOSED );
			pthread_exit( NULL );
			break;
		
		case C_ENQUIRE_LINK:
			esmelog->lprint( "[+] Got ENQUIRE_LINK: sequnce_number = %ld. Creating ENQUIRE_LINK_RESP.", pdu_sequence_number );
			make_pdu_header( \
				pdu_header, \
				PDU_HEADER_LENGTH, /* + sizeof(char),*/ \
				C_ENQUIRE_LINK_RESP, \
				ESME_ROK, \
				pdu_sequence_number \
			);
			pdu_header_exists = true;
			priority = PRIORITY_HIGH;
			break;

		case C_ENQUIRE_LINK_RESP:
			esmelog->lprint( "[+] Got ENQUIRE_LINK_RESP: sequence_number = %ld", pdu_sequence_number );
			enquire_timeouted = false;
			set_timeout( TIMEOUT_SELECT, LISTENER_TIMEOUT );
			break;	
		
		case C_GENERIC_NACK:
			esmelog->lprint( "Got GENERIC_NACK." );
			// TODO proceed (?) generic_nack
			// MTS doesn't want generic_nack
			break;

		case C_SUBMIT_SM_RESP:
			DEBUG_LOG( "case c_submit_sm_resp" );
			if ( pdu_command_length > PDU_HEADER_LENGTH )
			{
				char * ssmr_message_id = (char *)malloc( sizeof(char) * 65 );
				sock->rs( ssmr_message_id );
				free( ssmr_message_id );
			}
			esmelog->lprint( "[+] Got SUBMIT_SM_RESP for message with sequence_number = %ld and command_status = %ld. Updating submit_sm table in DB.", pdu_sequence_number, pdu_command_status );
			result = db_tlistener->query( "UPDATE submit_sm "
							 "SET error = '%ld' "
							 "WHERE sequence_id = '%ld'"
							 "  AND error IN (-2,20,88)",
							 pdu_command_status,
							 pdu_sequence_number
							);
			db_tlistener->freeresult( result );
			if ( pdu_command_status == ESME_RTHROTTLED )
			{
				esmelog->lprint( "[^] Got ESME_RTHROTTLED (throttling error). Stopping messages sending for %d seconds.", options->throttling_timeout );
				set_timeout(TIMEOUT_SUBMIT_SM, options->throttling_timeout);
			}
			break;
			
		case C_DELIVER_SM:
			char dsm_servtype[6];
			char dsm_src_addr[21];
			char dsm_dst_addr[21];
			char short_message[256];
			char * message = short_message;
			strcpy( short_message, "" );
			
			bool message_segmented;
			message_segmented = false;
			uint8 segment_ref;
			uint8 segment_number;
			uint8 segments_total;
			uint32 message_id;
			
			sock->rs( dsm_servtype );
			uint8 dsm_src_ton = sock->r8();
			uint8 dsm_src_npi = sock->r8();
			sock->rs( dsm_src_addr );
			uint8 dsm_dst_ton = sock->r8();
			uint8 dsm_dst_npi = sock->r8();
			sock->rs( dsm_dst_addr );
			uint8 dsm_esm_class = sock->r8();
			uint8 dsm_protocol_id = sock->r8();
			uint8 dsm_priority_flag = sock->r8();
			sock->dummy_read( 2 );
			uint8 dsm_registered_delivery = sock->r8();
			sock->dummy_read( 1 );
			uint8 dsm_data_coding = sock->r8();
			sock->dummy_read( 1 );
			uint32 dsm_sm_length = sock->r8();
			if ( dsm_sm_length )
			{
				uint8 rl = sock->rcs( short_message, dsm_sm_length );
				//printf( "short_message read: %d", rl );
			}

			esmelog->lprint( "[+] Got DELIVER_SM: length %d; sequence_number %d.", pdu_command_length, pdu_sequence_number );
			esmelog->lprint( "[^] \tservice_type: %d.", dsm_servtype );
			esmelog->lprint( "[^] \tsource_addr_ton: %d; source_addr_npi: %d; source_addr: %s.", dsm_src_ton, dsm_src_npi, dsm_src_addr );
			esmelog->lprint( "[^] \tdest_addr_ton: %d; dest_addr_npi: %d; destination_addr: %s.", dsm_dst_ton, dsm_dst_npi, dsm_dst_addr );
			esmelog->lprint( "[^] \tesm_class: %d; protocol_id: %d; priority_flag: %d.", dsm_esm_class, dsm_protocol_id, dsm_priority_flag );
			esmelog->lprint( "[^] \tregistered_delivery: %d; data_coding: %d; sm_length: %d.", dsm_registered_delivery, dsm_data_coding, dsm_sm_length );
			esmelog->lprint( "[^] \tshort_message: %s", short_message );
			
			if ( proceed_tlvs() == -1 )
				return;

			if (dsm_src_addr[0] == 0x00) {
				esmelog->lprint( "[-] PDU malformed: source_addr is empty. Ignoring." );
				return;
			}


			esmelog->lprint( "Read %d bytes.", sock->length_get() );

			sql_deliver_sm_insert[0] = 0x00;
			sql_insert_additional_args[0] = 0x00;
			sql_insert_additional_vals[0] = 0x00;
			
			for( i = 0; i < tlvs_count; ++i )
			{
				switch( tlvs[i].tag )
				{
					case TLV_MESSAGE_PAYLOAD:
						message = tlvs[i].c;
						dsm_sm_length = tlvs[i].length;
						break;
						
					case TLV_SAR_MSG_REF_NUM:
						message_segmented = true;
						segment_ref = tlvs[i].i8;
						break;

					case TLV_SAR_SEGMENT_SEQNUM:
						message_segmented = true;
						segment_number = tlvs[i].i8;
						break;

					case TLV_SAR_TOTAL_SEGMENTS:
						message_segmented = true;
						segments_total = tlvs[i].i8;
						break;	

					case TLV_MAXUS_REAL_MESSAGES_COUNT:
						uint32 msg_cnt = atol( tlvs[i].c );
						messages = msg_cnt;
						break;	

					case TLV_MAXUS_MESSAGE_ID:
						message_id = atol( tlvs[i].c );

					default:
						char * sql_insert_additional_temp = (char *)malloc( sizeof(char) * 1024 );
						if ( g_tlv_map[tlvs[i].tag].store_as > 1 )
						{
							switch( g_tlv_map[tlvs[i].tag].value_type )
							{
								case TLV_VALTYPE_UINT8:
									sprintf( sql_insert_additional_temp, value_types_args[g_tlv_map[tlvs[i].tag].value_type], tlvs[i].i8 );
									break;

								case TLV_VALTYPE_UINT16:
									sprintf( sql_insert_additional_temp, value_types_args[g_tlv_map[tlvs[i].tag].value_type], tlvs[i].i16 );
									break;
									
								case TLV_VALTYPE_UINT32:
									sprintf( sql_insert_additional_temp, value_types_args[g_tlv_map[tlvs[i].tag].value_type], tlvs[i].i32 );
									break;
									
								case TLV_VALTYPE_STRING:
									sprintf( sql_insert_additional_temp, value_types_args[g_tlv_map[tlvs[i].tag].value_type], tlvs[i].c );
									break;
									
							}

							strcat( sql_insert_additional_vals, sql_insert_additional_temp );

							strcat( sql_insert_additional_args, ", " );
							strcat( sql_insert_additional_args, g_tlv_map[tlvs[i].tag].column_name );
						}
						break;
				}
			}
			
			if ( in_message_len < dsm_sm_length + sizeof(char) )
			{
				in_message_len *= 2;
				in_message = (char *)realloc( (void *)in_message, sizeof(char) * in_message_len );
			}
			bzero(in_message,in_message_len);

			/*
			 * UDH processing.
			 * Why there is no any information about UDH in SMPP 3.4 Specification?! *angry*
			 * But there is: http://www.isms.ru/article.shtml?art_12
			 *
			 * It's not best way to process UDH, I know. But... I'm too tired.
			 */
			if ( dsm_esm_class & 0x40 ) {
/*				printf("\nIt's seems to be that our message is UDH. Hexdump:");
				int i;
				for( i = 0; i < dsm_sm_length; i++ ) {
					printf( "%02X ", message[i] );
				}
				printf("\n");*/

				if ( message[1] ) {
					segment_ref = ntohs( *( (uint16*)(&message[3]) ) );
					message++;
					dsm_sm_length--;
				}
				else {
					segment_ref = message[3];
				}
				segment_number = message[5];
				segments_total = message[4];
				esmelog->lprint( "[^] UDH: segment_ref: %d; segment_number: %d; segments_total: %d.", segment_ref, segment_number, segments_total );
				message_segmented = true;

				message += 6;
				dsm_sm_length -= 6;
			}

			message_decode( message, dsm_sm_length, in_message, dsm_data_coding );

			if ( message_segmented )
			{
				esmelog->lprint( "[^] \tDELIVER_SM message segment (ref: %d; count: %d; total: %d): %s", segment_ref, segment_number, segments_total, in_message );
				segments->push( segment_ref, segment_number, in_message );
				if ( segments->getsegcnt( segment_ref ) < segments_total )
				{
					esmelog->lprint( "[^] Message segmented and it's not last segment." );
					//strcpy( in_message, "" );
					//in_message[0] = 0x00;
				}
				else
				{
					size_t reslen = segments->getreslen( segment_ref );
					if ( in_message_len < reslen )
					{
						in_message_len = reslen * 4;
						in_message = (char *)realloc( (void *)in_message, sizeof(char) * (reslen + 1) );
					}

					segments->pop( segment_ref, in_message );
				}
			}

			esmelog->lprint( "[^] \tFull DELIVER_SM message (decoded): %s", in_message );

			make_pdu_header( \
				pdu_header, \
				PDU_HEADER_LENGTH + sizeof(char), \
				C_DELIVER_SM_RESP, \
				ESME_ROK, \
				pdu_sequence_number \
			);
			pdu_header_exists = true;
			priority = PRIORITY_HIGH;
			
			m_opack->lock();
			o_pack->clear();
			o_pack->w8( 0 );
			body_length = o_pack->out(pdu_body);
			o_pack->clear();
			m_opack->unlock();

			esmelog->lprint( "[+] DELIVER_SM_RESP created; sequence_number = %d.", pdu_sequence_number );

			if (message_segmented && segments->getsegcnt(segment_ref) < segments_total) {
				break;
			}

			// Dummy worckaround to Svyaznoy Timeout
			result = db_tlistener->query( "SELECT id FROM deliver_sm WHERE msgid = '%d'", message_id );
			if (db_tlistener->num_rows(result)) {
				if ( result > -1 )
                                        db_tlistener->freeresult(result);
				esmelog->lprint( "[!] DELIVER_SM DUP! for message id: %d", message_id );
				break;
			}
			result = db_tlistener->query( DELIVER_SM_INSERT_TEMPLATE, sql_insert_additional_args, \
								esmeid, smsc_id(), \
								dsm_servtype, \
								dsm_src_ton, dsm_src_npi, dsm_src_addr, \
								dsm_dst_ton, dsm_dst_npi, dsm_dst_addr, \
								dsm_esm_class, dsm_protocol_id, dsm_priority_flag, dsm_data_coding, sqlesc(in_message).c_str(), \
								sql_insert_additional_vals 
							);
			if ( result > -1 )
				db_tlistener->freeresult( result );

			if (!messages) {
				int messlen = strlen(in_message);
				messages = messlen / MESS_ATOM_LENGTH(dsm_data_coding) + (messlen % MESS_ATOM_LENGTH(dsm_data_coding) ? 1 : 0);
			}

			while (--messages > 0) {
				result = db_tlistener->query( DELIVER_SM_INSERT_TEMPLATE, sql_insert_additional_args, \
						esmeid, smsc_id(), \
						dsm_servtype, \
						dsm_src_ton, dsm_src_npi, dsm_src_addr, \
						dsm_dst_ton, dsm_dst_npi, dsm_dst_addr, \
						dsm_esm_class, dsm_protocol_id, dsm_priority_flag, dsm_data_coding, "OPgUZNLg_DUMMY", \
						sql_insert_additional_vals 
				);

				if ( result > -1 )
					db_tlistener->freeresult(result);
			}

			esmelog->lprint("[+] Message with sequence_number %d inserted in DB.", pdu_sequence_number);
			break;
			

		default:
			esmelog->lprint( "Got some PDU with 0x%08X id.", pdu_command_id );
			
			if ( pdu_command_length - PDU_HEADER_LENGTH )
				sock->dummy_read( pdu_command_length - PDU_HEADER_LENGTH );
			break;
	}
	
	if ( pdu_header_exists ) {
		insert_pdu_in_out_queue( pdu_header, pdu_body, body_length, priority );
		esmelog->lprint( "[+] PDU inserted in output queue; sequence_number = %d.", pdu_sequence_number );
	}
		
	free_tlvs_dynarg();
}

int
smsc::proceed_tlvs()
{
	size_t _tlvs_length = pdu_command_length - sock->length_get();
	
	if ( _tlvs_length < 1 )
	{
		tlvs_count = 0;
		return 0;
	}
	
	if ( _tlvs_length < TLV_MIN_SIZE )
	{
		tlvs_count = 0;
		esmelog->lprint( "[-] SMSC: proceed_tlvs: _tlvs_length got from arguments is fewer than TLV_MIN_SIZE (4 bytes). Reading out and returning." );
		sock->dummy_read( _tlvs_length );
		return 0;
	}
		
	tlv_t _tlv;
	size_t _tlv_length;
	size_t _ldec = 0;
	
	size_t tlvs_size_req = (_tlvs_length / (TLV_MIN_SIZE + 1));
	if ( tlvs_size < tlvs_size_req )
	{
		tlvs_size = tlvs_size_req * 2;
		tlvs = (tlv_t *)realloc( tlvs, sizeof(tlv_t) * tlvs_size );
	}
	
	tlvs_count = 0;
	while (_tlvs_length)
	{
		_tlv.tag = sock->r16h();
		_tlv.length = sock->r16h();
		_ldec = 0;
		
		DEBUG_LOG( "reading tlv \n" )
		
		if ( _tlv.length )
		{
			_tlv_length = _tlv.length;
			
			strcpy( log_val, "no value" );
			
			switch( g_tlv_map[_tlv.tag].value_type )
			{
				case TLV_VALTYPE_UINT8:
					_tlv.i8 = sock->r8();
					_ldec = sizeof(uint8);
					sprintf( log_val, "%d", _tlv.i8 );
					break;
				case TLV_VALTYPE_UINT16:
					_tlv.i16 = sock->r16h();
					_ldec = sizeof(uint16);
					sprintf( log_val, "%d", _tlv.i16 );
					break;
				case TLV_VALTYPE_UINT32:
					_tlv.i32 = sock->r32h();
					_ldec = sizeof(uint32);
					sprintf( log_val, "%d", _tlv.i32 );
					break;
				case TLV_VALTYPE_STRING:
					_tlv.c = (char *)malloc( sizeof(char) * (_tlv.length + 1) );
					_ldec = sock->rcs( _tlv.c, _tlv.length );
					if (log_val_size < _tlv.length)
						log_val = (char*)realloc((void*)log_val, sizeof(char) * (log_val_size = _tlv_length + 1));
					strcpy( log_val, _tlv.c );
					break;
				
				default:
					esmelog->lprint( "[-] Unknown value_type for TLV with tag 0x%04X, but there is %d bytes of data left. Reading out and hoping for the best. TLV won't be stored.", _tlv.tag, _tlv.length );
					if ( (_ldec = sock->dummy_read( _tlv.length )) <= 0 ) {
						esmelog->lprint( "[-] Dummy read failed. Going in state CLOSED_BY_ERR and reconnecting in %d seconds.", options->rebind_timeout );
						set_timeout(TIMEOUT_BIND, options->rebind_timeout);
						set_state(SMSC_STATE_CLOSED_BY_ERR);
						sock->disconn();
						return -1;
					}
					continue;
			}
			
			if ( (_tlv_length - _ldec) > 0 )
			{
				esmelog->lprint( "[-] Strange: I read TLV value according to tlv_map, but there is %d bytes of data left for TLV with tag 0x%04X. Reading out and hoping for the best. TLV will be stored.", _tlv_length, _tlv.tag );
				_ldec += sock->dummy_read( _tlv_length - _ldec );
			}
			
			esmelog->lprint( "[+] Got TLV: tag 0x%04X (%s), length %d, value %s", _tlv.tag, g_tlv_map[_tlv.tag].description, _tlv.length, log_val );
			
			tlvs[tlvs_count++] = _tlv;
			DEBUG_LOG( "tlv stored\n" )
		}
		
		_tlvs_length -= _ldec + sizeof(uint16) * 2;
		DEBUG_LOG( "_tlvs_length decreased\n" )
	}
	DEBUG_LOG( "exiting proceed_tlvs\n" );
	return 0;
}

void
smsc::free_tlvs_dynarg()
{
	size_t i = 0;
	for ( ; i < tlvs_count; ++i )
	{
		if ( tlvs[i].length > TLV_STRING_MAX_LEN )
			free( tlvs[i].c );
	}
}

int
smsc::message_decode( char * _message, size_t _len, char * _buffer, uint8 _encoding )
{
	if ( _encoding != DC_UCS2BE && _encoding != DC_UNI )	
	{
		strcpy( _buffer, _message );
		return _len;
	}

//	size_t _lend = _len / 2;
	return iconv(iv_decode, &_message, &_len, &_buffer, &_len);
}

int
smsc::message_encode( char * _message, size_t _len, char * _buffer, uint8 _encoding )
{
	if ( _encoding != DC_UCS2BE && _encoding != DC_UNI )	
	{
		strcpy( _buffer, _message );
		return strlen(_message);
	}

	size_t _lend = _len * 2;
	size_t _retval = _lend;
	iconv(iv_encode, &_message, &_len, &_buffer, &_lend);
	return _retval;
}

void
smsc::insert_pdu_in_out_queue( char * _pdu_header, char * _pdu, size_t _body_length, char _priority )
{
	DEBUG_LOG( "insert_pdu_in_out_queue\n" );

	char * packet;
	queue_elem_t qelem;
	
	m_opack->lock();
	o_pack->clear();
	o_pack->pput( _pdu_header, PDU_HEADER_LENGTH );
	if ( _pdu && _body_length )
		o_pack->pput( _pdu, _body_length );
	
	qelem.data_len = o_pack->size();
	packet = (char *)malloc( sizeof(char) * qelem.data_len );
	
	o_pack->out( packet );
	o_pack->clear();
	m_opack->unlock();
	
	qelem.data = packet;
	
	DEBUG_LOG( "insert_pdu_in_out_queue: out_queue.push()\n" );
	out_queue.push( qelem, _priority );
	
	DEBUG_LOG( "insert_pdu_in_out_queue: done\n" );
}

/*
 * make_pdu_* functions family will allocate _buffer
 */
void
smsc::make_pdu_header( char * _buffer, uint32 _command_length, uint32 _command_id, uint32 _command_status, uint32 _sequence_number )
{
	m_opack->lock();
	o_pack->clear();
	o_pack->w32n( _command_length );
	o_pack->w32n( _command_id );
	o_pack->w32n( _command_status );
	o_pack->w32n( _sequence_number );
	
	o_pack->out( _buffer );
	o_pack->clear();
	m_opack->unlock();
}

void
smsc::make_bind_pdu()
{
	DEBUG_LOG( "make_bind_pdu\n" );
	
	char * pdu_header = (char *)malloc( PDU_HEADER_LENGTH );
	uint32 _command_id;
	
	switch(options->bind_type)
	{
		case BIND_TYPE_TX:
			_command_id = C_BIND_TRANSMITTER;
			break;
		case BIND_TYPE_RX:
			_command_id = C_BIND_RECEIVER;
			break;
		case BIND_TYPE_TXRX:
			_command_id = C_BIND_TRANSCEIVER;
			break;
	}
	
	size_t len;
	m_opack->lock();
	o_pack->clear();
	
	o_pack->ws( options->system_id );
	o_pack->ws( options->password );
	o_pack->ws( options->system_type );
	o_pack->w8( options->interface_version );
	o_pack->w8( options->addr_ton );
	o_pack->w8( options->addr_npi );
	o_pack->ws( options->address_range );
	
	resize_pdu_body( o_pack->size() );
	len = o_pack->out( pdu_body );
	o_pack->clear();
	m_opack->unlock();
	
	DEBUG_LOG( "make_pdu_header\n" );
	make_pdu_header( pdu_header, len + PDU_HEADER_LENGTH, _command_id, ESME_RNULL, roll_seqnum() );
	
	insert_pdu_in_out_queue( pdu_header, pdu_body, len, PRIORITY_NORMAL );

	free(pdu_header);
}

int
smsc::writer()
{
	queue_elem_t * packet;
	
	if ( get_state() == SMSC_STATE_CONNECTED )
	{
		DEBUG_LOG( "clearing out_queue\n" );
		out_queue.clear();
		make_bind_pdu();
		esmelog->lprint( "[+] BIND PDU added to queue." );
		DEBUG_LOG( "writer: changing state to SMSC_STATE_CONNECTED_WAIT_BIND_RESP" );
		set_state(SMSC_STATE_CONNECTED_WAIT_BIND_RESP);
	}
	
	while (packet = out_queue.top())
	{
		// Strange construction below gets current PDU command_id
		if ( ntohl(*((unsigned int *)(packet->data + sizeof(uint32)))) != C_SUBMIT_SM || !get_timeout(TIMEOUT_SUBMIT_SM) )
		{
			esmelog->lprint( "[+] PDU sending to socket; sequence_number = %d.", ntohl(*((uint32 *)(packet->data + 3*sizeof(uint32)))) );
			if ( sock->snd( packet->data, packet->data_len ) < 0 )
			{
				esmelog->lprint( "[-] Cannot write to socket. Disconnecting and reconnecting in %d seconds.", options->rebind_timeout );
				sock->disconn();
				set_timeout(TIMEOUT_BIND, options->rebind_timeout);
				set_state(SMSC_STATE_CLOSED_BY_ERR);
				return IESME_ERROR_OK;
			}
			esmelog->lprint( "[+] PDU sent to socket; sequence_number = %d.", ntohl(*((uint32 *)(packet->data + 3*sizeof(uint32)))) );
		}
		free(packet->data);
		out_queue.pop();
	}
		
	return IESME_ERROR_OK;
} 

int
smsc::db_listener()
{

	if ( !was_submit_sm_timeout && !db_listener_first_time )
	{
		switch(db_listen->listen(0))
		{
			case -1:
				return IESME_ERROR_FAILED;
			case 0:
				return IESME_ERROR_OK;
		}
	}
	
	switch( get_state() )
	{
		case SMSC_STATE_BOUND_TXRX:
		case SMSC_STATE_BOUND_TX:
			break;

		default:
			was_submit_sm_timeout = true;
			return 0;	
	}

	if ( get_timeout(TIMEOUT_SUBMIT_SM) )
	{
		was_submit_sm_timeout = true;
		return IESME_ERROR_OK;
	}

	esmelog->lprint( "[+] Recieved notification submit_sm." );
	
	int result;
	if (esmeid == 5 && smsc_id() == 22) {				// Fuckin' workaround for TELE2 messages. I don't like it hard-coded, but it's the rightest way to "multiplex" incore link.
		result = db_listen->query( "SELECT * FROM submit_sm "
				       "WHERE esmeid IN (5,7)"
				       "  AND smscid IN (18,22)"
				       "  AND error IN (-1)"
				);
	}
	else {
		result = db_listen->query( "SELECT * FROM submit_sm "
				       "WHERE esmeid = %d"
				       "  AND smscid = %d"
				       "  AND error IN (-1)",
		       			esmeid,
		       			smsc_id()
				);
	}
//	esmelog->lprint( "[ ] SELECT * FROM submit_sm WHERE esmeid = %d AND smscid = %d AND error IN (-1, 20, 88)", esmeid, smsc_id() );
	esmelog->lprint( "[+] Read %d SMSes from submit_sm, creating pdus.", db_listen->num_rows(result) );

	pgrow_t* prow = NULL;
	while( prow = db_listen->fetch_row( result ) )
	{
		pgrow_t& row = *prow;

		int orig_message_len = strlen(row["mess"].c_str());
		if ( out_message_len < ((orig_message_len + 1) * 2) )
		{
			out_message_len *= ((orig_message_len + 1) * 2) * 2;
			out_message = (char *)realloc( (void *)out_message, out_message_len );
		}

		uint8 ssm_data_coding = atoi( row["data_coding"].c_str() );

//		printf( "orig_mess_len: %d\n", orig_message_len );
		int message_len = message_encode( (char *)row["mess"].c_str(), orig_message_len, out_message, ssm_data_coding );

/*		if ( message_len == (size_t)(-1) )
			printf( "iconv error!\n" );
		else
			printf( "db_listener: new message_len (after encoding) = %d\n", message_len );*/

		s_pack->clear();
		s_pack->ws( row["serv_type"].c_str() );
		s_pack->w8( atoi(row["srcaddr_ton"].c_str()) );
		s_pack->w8( atoi(row["srcaddr_npi"].c_str()) );
		s_pack->ws( row["src_addr"].c_str() );
		s_pack->w8( atoi(row["dstaddr_ton"].c_str()) );
		s_pack->w8( atoi(row["dstaddr_npi"].c_str()) );
		s_pack->ws( row["dst_addr"].c_str() );
		s_pack->w8( 0x00 ); // esm_class
		s_pack->w8( atoi(row["protocol_id"].c_str()) );
		s_pack->w8( atoi(row["priority_flag"].c_str()) );
		s_pack->w8( 0x00 ); // schedule_delivery_time
		s_pack->w8( 0x00 ); // validity_period (default, set by SMSC)
		s_pack->w8( 0x00 ); // registered_delivery
		s_pack->w8( 0x00 ); // replace_if_present_flag
	       	s_pack->w8( ssm_data_coding );
		s_pack->w8( 0x00 ); // sm_default_msg_id
		if ( message_len <= 254 )
		{
			s_pack->w8( message_len );
			s_pack->pput( out_message, message_len );
		}
		else
		{
			s_pack->w8( 0x00 );
		}

		for ( uint16 i = 0; i < tlvs_max_tag; ++i )
		{
			if ( g_tlv_map[i].store_as > TLV_DONT_STORE )
			{
				if ( i == TLV_MESSAGE_PAYLOAD && message_len <= 254 )
						continue;
				uint16 tlv_val_len = 0;
				s_pack->w16n( i );
				switch( g_tlv_map[i].value_type )
				{
					case TLV_VALTYPE_UINT8:
						s_pack->w16n( sizeof(uint8) );
						s_pack->w8( atoi(row[g_tlv_map[i].column_name].c_str()) );
						tlv_val_len = sizeof(uint8);
						break;
					case TLV_VALTYPE_UINT16:
						s_pack->w16n( sizeof(uint16) );
						s_pack->w16n( atoi(row[g_tlv_map[i].column_name].c_str()) );
						tlv_val_len = sizeof(uint16);
						break;
					case TLV_VALTYPE_UINT32:
						s_pack->w16n( sizeof(uint32) );
						s_pack->w32n( atol(row[g_tlv_map[i].column_name].c_str()) );
						tlv_val_len = sizeof(uint32);
						break;
					case TLV_VALTYPE_STRING:
						if ( i == TLV_MESSAGE_PAYLOAD )
						{
							s_pack->w16n( message_len );
							s_pack->pput( out_message, message_len );
							tlv_val_len = message_len; 
							break;
						}
						tlv_val_len = strlen( row[g_tlv_map[i].column_name].c_str() );
						s_pack->w16n( tlv_val_len );
						s_pack->wcs( row[g_tlv_map[i].column_name].c_str() );
						break;
				}
				esmelog->lprint( "[+] Attached TLV %s: length %d; value \"%s\".", g_tlv_map[i].description, tlv_val_len, row[g_tlv_map[i].column_name].c_str() );
			}
		}

		if ( ssm_body_size < s_pack->size() )
		{
			ssm_body_size = s_pack->size() * 2;
			ssm_body = (char *)realloc( (void *)ssm_body, ssm_body_size );
		}

		uint32 ssm_body_length = s_pack->out( ssm_body );
		uint32 ssm_sequence_number = roll_seqnum();

		s_pack->clear();
		s_pack->w32n( ssm_body_length + PDU_HEADER_LENGTH );
		s_pack->w32n( C_SUBMIT_SM );
		s_pack->w32n( ESME_ROK );
		s_pack->w32n( ssm_sequence_number );
		s_pack->out( ssm_header );
		s_pack->clear();

		int uresult = db_listen->query( "UPDATE submit_sm "
			       			"SET error = -2,"
						"    sequence_id = %ld "
						"WHERE id = %ld",
			       				ssm_sequence_number,
							atol(row["id"].c_str())
						);
		db_listen->freeresult( uresult );

		esmelog->lprint( "[+] Created message \"%s\" for %s. Putting it in queue. sequence_number = %d", row["mess"].c_str(), row["dst_addr"].c_str(), ssm_sequence_number );

		insert_pdu_in_out_queue( ssm_header, ssm_body, ssm_body_length, PRIORITY_NORMAL );
	}
	db_listener_first_time = \
	was_submit_sm_timeout = false;
}

/*
 * Thread functions
 */

void *
smsc::_listener( void * arg )
{
	smsc * obj = (smsc *)arg;
	bool l_continue;
	
	DEBUG_LOG( "[+] _listener thread started.\n" );
	
	while( obj->listener_alive )
	{
		l_continue = false;
		switch( obj->get_state() )
		{
			case SMSC_STATE_CLOSED:
			case SMSC_STATE_CLOSED_BY_ERR:
			case SMSC_STATE_CLOSED_NEED_CONNECT:
			case SMSC_STATE_CONNECTED:
			case SMSC_STATE_UNBIND_BY_SMSC:
			case SMSC_STATE_AWAITING_START:
				l_continue = true;
		}
		if ( l_continue ) {
			usleep( 1000 );
			continue;
		}
			
		if ( obj->listener() )
			obj->listener_alive = false;
	}
	
	DEBUG_LOG( "[+] _listener thread exiting.\n" );
	
	pthread_exit( NULL );
}

void *
smsc::_writer( void * arg )
{
	bool l_continue;
	smsc * obj = (smsc *)arg;
	
	DEBUG_LOG( "[+] _writer thread started.\n" );
	
	while( obj->writer_alive )
	{
		usleep( 1000 );
		
		l_continue = false;
		switch( obj->get_state() )
		{
			case SMSC_STATE_CLOSED:
			case SMSC_STATE_CLOSED_BY_ERR:
			case SMSC_STATE_CLOSED_NEED_CONNECT:
			case SMSC_STATE_CONNECTED_WAIT_BIND_RESP:
			case SMSC_STATE_WAIT_UNBIND_RESP:
			case SMSC_STATE_AWAITING_START:
				l_continue = true;
		}
		
		if ( l_continue )
			continue;
			
		if ( obj->writer() )
			obj->writer_alive = false;
	}
	
	DEBUG_LOG( "[+] _writer thread exiting.\n" );
	
	pthread_exit( NULL );
}

void *
smsc::_db_listener( void * arg )
{
	smsc * obj = (smsc *)arg;
	bool l_continue = false;
	
	DEBUG_LOG( "[+] _listener thread started.\n" );
	
	while( obj->db_listener_alive )
	{
		l_continue = true;
		switch( obj->get_state() )
		{
			case SMSC_STATE_BOUND_TXRX:
			case SMSC_STATE_BOUND_TX:
				l_continue = false;	
		}

		if ( l_continue ) {
			usleep( 1000 );
			continue;
		}

		if ( obj->db_listener() < 0 )
			obj->db_listener_alive = false;	
	}
	
	DEBUG_LOG( "[+] _db_listener thread exiting.\n" );
	
	pthread_exit( NULL );
}

void
smsc::timer()
{
	if ( get_timeout(TIMEOUT_SUBMIT_SM) )
		set_timeout(TIMEOUT_SUBMIT_SM, get_timeout(TIMEOUT_SUBMIT_SM) - 1);
	if ( get_timeout(TIMEOUT_BIND) )
		set_timeout(TIMEOUT_BIND, get_timeout(TIMEOUT_BIND) - 1);
}


void
smsc::resize_pdu_body( size_t req_len )
{
	if ( req_len <= pdu_body_length )
		return;
	
	pdu_body_length *= 2;
	pdu_body = (char *)realloc( pdu_body, sizeof(char) * pdu_body_length );
}

uint8
smsc::get_state()
{
	m_state->lock();
	uint8 ret = smsc_state;
	m_state->unlock();
	return ret;
}

void
smsc::set_state( uint8 new_state )
{
	m_state->lock();
	smsc_state = new_state;
	m_state->unlock();

	int result = db->query( "UPDATE smsc "
				"SET state = %d "
				"WHERE esmeid = %d "
				"  AND id = %d",
				 new_state,
				 esmeid,
				 smsc_id() );
	if ( result > -1 )
		db->freeresult( result );
}
	
void
smsc::set_timeout( uint8 _timeout_tag, uint32 _new_value )
{
	m_timeouts->lock();
	timeouts[_timeout_tag] = _new_value;
	m_timeouts->unlock();
}

uint32
smsc::get_timeout( uint8 _timeout_tag )
{
	m_timeouts->lock();
	uint32 ret = timeouts[_timeout_tag];
	m_timeouts->unlock();
	
	return ret;
}

uint32
smsc::roll_seqnum()
{
	m_outseqnum->lock();	
	if ( !out_sequence_number )
		out_sequence_number = 1;

	uint32 ret = out_sequence_number++;
	m_outseqnum->unlock();	

	return ret;
}

void
smsc_init_constants()
{
	value_types_args[1] = \
	value_types_args[2] = \
	value_types_args[3] = ", '%d'";
	value_types_args[4] = ", '%s'";

	bind_string[C_BIND_RECEIVER_RESP] = "BIND_RECEIVER_RESP";
	bind_string[C_BIND_TRANSCEIVER_RESP] = "BIND_TRANSCEIVER_RESP";
	bind_string[C_BIND_TRANSMITTER_RESP] = "BIND_TRANSMITTER_RESP";
	
	/* Bind errors */
	error_desc[ESME_RALYBND] = "ESME_RALYBND (ESME already in bound state)";
	error_desc[ESME_RBINDFAIL] = "ESME_RBINDFAIL (Bind failed)";
	error_desc[ESME_RINVPASWD] = "ESME_RINVPASSWD (Invalid password)";
	error_desc[ESME_RINVSYSID] = "ESME_RINVSYSID (Invalid system_id)";
}

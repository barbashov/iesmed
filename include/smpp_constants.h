#ifndef __SMPP_CONSTANTS_H__
#define __SMPP_CONSTANTS_H__

#define GSM_ADDR_TON_UNKNOWN          0x00000000
#define GSM_ADDR_TON_INTERNATIONAL    0x00000001
#define GSM_ADDR_TON_NATIONAL         0x00000002
#define GSM_ADDR_TON_NETWORKSPECIFIC  0x00000003
#define GSM_ADDR_TON_SUBSCRIBER       0x00000004
#define GSM_ADDR_TON_ALPHANUMERIC     0x00000005 /* GSM TS 03.38 */
#define GSM_ADDR_TON_ABBREVIATED      0x00000006
#define GSM_ADDR_TON_EXTENSION        0x00000007 /* Reserved */

#define GSM_ADDR_NPI_UNKNOWN          0x00000000
#define GSM_ADDR_NPI_E164             0x00000001
#define GSM_ADDR_NPI_X121             0x00000003
#define GSM_ADDR_NPI_TELEX            0x00000004
#define GSM_ADDR_NPI_NATIONAL         0x00000008
#define GSM_ADDR_NPI_PRIVATE          0x00000009
#define GSM_ADDR_NPI_ERMES            0x0000000A /* ETSI DE/PS 3 01-3 */
#define GSM_ADDR_NPI_EXTENSION        0x0000000F /* Reserved */

/******************************************************************************
 * esm_class parameters for both submit_sm and deliver_sm PDUs
 */
#define ESM_CLASS_SUBMIT_DEFAULT_SMSC_MODE        0x00000000
#define ESM_CLASS_SUBMIT_DATAGRAM_MODE            0x00000001
#define ESM_CLASS_SUBMIT_FORWARD_MODE             0x00000002
#define ESM_CLASS_SUBMIT_STORE_AND_FORWARD_MODE   0x00000003
#define ESM_CLASS_SUBMIT_DELIVERY_ACK             0x00000008
#define ESM_CLASS_SUBMIT_USER_ACK                 0x00000010
#define ESM_CLASS_SUBMIT_UDH_INDICATOR            0x00000040
#define ESM_CLASS_SUBMIT_RPI                      0x00000080
#define ESM_CLASS_SUBMIT_UDH_AND_RPI              0x000000C0

#define ESM_CLASS_DELIVER_DEFAULT_TYPE            0x00000000
#define ESM_CLASS_DELIVER_SMSC_DELIVER_ACK        0x00000004
#define ESM_CLASS_DELIVER_SME_DELIVER_ACK         0x00000008
#define ESM_CLASS_DELIVER_SME_MANULAL_ACK         0x00000010
#define ESM_CLASS_DELIVER_INTERM_DEL_NOTIFICATION 0x00000020

#define c_generic_nack             0x80000000
#define c_bind_receiver            0x00000001
#define c_bind_receiver_resp       0x80000001
#define c_bind_transmitter         0x00000002
#define c_bind_transmitter_resp    0x80000002
#define c_query_sm                 0x00000003
#define c_query_sm_resp            0x80000003
#define c_submit_sm                0x00000004
#define c_submit_sm_resp           0x80000004
#define c_deliver_sm               0x00000005
#define c_deliver_sm_resp          0x80000005
#define c_unbind                   0x00000006
#define c_unbind_resp              0x80000006
#define c_replace_sm               0x00000007
#define c_replace_sm_resp          0x80000007
#define c_cancel_sm                0x00000008
#define c_cancel_sm_resp           0x80000008
#define c_bind_transceiver         0x00000009
#define c_bind_transceiver_resp    0x80000009
#define c_outbind                  0x0000000B
#define c_enquire_link             0x00000015
#define c_enquire_link_resp        0x80000015
#define c_submit_multi             0x00000021
#define c_submit_multi_resp        0x80000021
#define c_alert_notification       0x00000102
#define c_data_sm                  0x00000103
#define c_data_sm_resp             0x80000103

#define DC_UNDEF 0
#define DC_7BIT 1
#define DC_8BIT 2
#define DC_BIN 0x04
#define DC_UCS2 8

#define SMS_7BIT_MAX_LEN 160
#define SMS_8BIT_MAX_LEN 140
#define SMS_UCS2_MAX_LEN 70

#define DEST_ADDR_SUBUNIT           0x0005
#define DEST_NETWORK_TYPE           0x0006
#define DEST_BEARER_TYPE            0x0007
#define DEST_TELEMATICS_ID          0x0008
#define SOURCE_ADDR_SUBUNIT         0x000D
#define SOURCE_NETWORK_TYPE         0x000E
#define SOURCE_BEARER_TYPE          0x000F
#define SOURCE_TELEMATICS_ID        0x0010
#define QOS_TIME_TO_LIVE            0x0017
#define PAYLOAD_TYPE                0x0019
#define ADDITIONAL_STATUS_INFO_TEXT 0x001D
#define RECEIPTED_MESSAGE_ID        0x001E
#define MS_MSG_WAIT_FACILITIES      0x0030
#define PRIVACY_INDICATOR           0x0201
#define SOURCE_SUBADDRESS           0x0202
#define DEST_SUBADDRESS             0x0203
#define USER_MESSAGE_REFERENCE      0x0204
#define USER_RESPONSE_CODE          0x0205
#define SOURCE_PORT                 0x020A
#define DESTINATION_PORT            0x020B
#define SAR_MSG_REF_NUM             0x020C
#define LANGUAGE_INDICATOR          0x020D
#define SAR_TOTAL_SEGMENTS          0x020E
#define SAR_SEGMENT_SEQNUM          0x020F
#define SC_INTERFACE_VERSION        0x0210
#define CALLBACK_NUM_PRES_IND       0x0302
#define CALLBACK_NUM_ATAG           0x0303
#define NUMBER_OF_MESSAGES          0x0304
#define CALLBACK_NUM                0x0381
#define DPF_RESULT                  0x0420
#define SET_DPF                     0x0421
#define MS_AVAILABILITY_STATUS      0x0422
#define NETWORK_ERROR_CODE          0x0423
#define MESSAGE_PAYLOAD             0x0424
#define DELIVERY_FAILURE_REASON     0x0425
#define MORE_MESSAGES_TO_SEND       0x0426
#define MESSAGE_STATE               0x0427
#define USSD_SERVICE_OP             0x0501
#define DISPLAY_TIME                0x1201
#define SMS_SIGNAL                  0x1203
#define MS_VALIDITY                 0x1204
#define ALERT_ON_MESSAGE_DELIVERY   0x130C
#define ITS_REPLY_TYPE              0x1380
#define ITS_SESSION_INFO            0x1383
#define DELIVER_ROUTE               0x1402
#define SMS_ID                      0x1420
#define SERVICE_ID                  0x1421
#define REAL_SMS_NUMBER             0x1430


#endif
#ifndef __SMPP_H_
#define __SMPP_H_

#define PDU_HEADER_LENGTH	sizeof(uint32) * 4

/*
 * Commands
 */
#define C_GENERIC_NACK		0x80000000	/* In */
#define C_BIND_RECEIVER		0x00000001	/* Out + */
#define C_BIND_RECEIVER_RESP	0x80000001	/* In  + */
#define C_BIND_TRANSMITTER	0x00000002	/* Out + */
#define C_BIND_TRANSMITTER_RESP	0x80000002	/* In  + */
#define C_QUERY_SM		0x00000003	/* Out - */
#define C_QUERY_SM_RESP		0x80000003	/* In  - */
#define C_SUBMIT_SM		0x00000004	/* Out */
#define C_SUBMIT_SM_RESP	0x80000004	/* In */
#define C_DELIVER_SM		0x00000005	/* In */
#define C_DELIVER_SM_RESP	0x80000005	/* Out */
#define C_UNBIND		0x00000006	/* In  + Out */
#define C_UNBIND_RESP		0x80000006	/* In  + Out */
#define C_REPLACE_SM		0x00000007	/* Out - */
#define C_REPLACE_SM_RESP	0x80000007	/* In  - */
#define C_CANCEL_SM		0x00000008	/* Out - */
#define C_CANCEL_SM_RESP	0x80000008	/* In  - */
#define C_BIND_TRANSCEIVER	0x00000009	/* Out + */
#define C_BIND_TRANSCEIVER_RESP	0x80000009	/* In  + */
#define C_OUTBIND		0x0000000B	/* In */
#define C_ENQUIRE_LINK		0x00000015	/* In  + Out */
#define C_ENQUIRE_LINK_RESP	0x80000015	/* In    Out + */
#define C_SUBMIT_MULTI		0x00000021	/* Out - */
#define C_SUBMIT_MULTI_RESP	0x80000021	/* In  - */
#define C_ALERT_NOTIFICATION	0x00000102	/* In  - */
#define C_DATA_SM		0x00000103	/* In  - */
#define C_DATA_SM_RESP		0x80000103	/* Out - */

/*
 * Error constants
 */
#define ESME_ROK		0x00000000
#define ESME_RNULL		ESME_ROK
#define ESME_RINVMSGLEN		0x00000001
#define ESME_RINVCMDLEN		0x00000002
#define ESME_RINVCMDID		0x00000003
#define ESME_RINVBNDSTS		0x00000004
#define ESME_RALYBND		0x00000005
#define ESME_RINVPRTFLG		0x00000006
#define ESME_RINVREGDLVFLG	0x00000007
#define ESME_RSYSERR		0x00000008
#define ESME_RINVSRCADR		0x0000000A
#define ESME_RINVDSTADDR	0x0000000B
#define ESME_RINVMSGID		0x0000000C
#define ESME_RBINDFAIL		0x0000000D
#define ESME_RINVPASWD		0x0000000E
#define ESME_RINVSYSID		0x0000000F
#define ESME_RCANCELFAIL	0x00000011
#define ESME_RREPLACEFAIL	0x00000013
#define ESME_RMSGQFUL		0x00000014
#define ESME_RINVSERTYP		0x00000015
#define ESME_RINVNUMDESTS	0x00000033
#define ESME_RINVDLNAME		0x00000034
#define ESME_RINVDESTFLAG	0x00000040
#define ESME_RINVSUBREP		0x00000042
#define ESME_RINVESMCLASS	0x00000043
#define ESME_RCNTSUBDL		0x00000044
#define ESME_RSUBMITFAIL	0x00000045
#define ESME_RINVSRCTON		0x00000048
#define ESME_RINVSRCNPI		0x00000049
#define ESME_RINVDSTTON		0x00000050
#define ESME_RINVDSTNPI		0x00000051
#define ESME_RINVSYSTYP		0x00000053
#define ESME_RINVREPFLAG	0x00000054
#define ESME_RINVNUMMSGS	0x00000055
#define ESME_RTHROTTLED		0x00000058
#define ESME_RINVSCHED		0x00000061
#define ESME_RINVEXPIRY		0x00000062
#define ESME_RINVDFTMSGID	0x00000063
#define ESME_RX_T_APPN		0x00000064
#define ESME_RX_P_APPN		0x00000065
#define ESME_RX_R_APPN		0x00000066
#define ESME_RQUERYFAIL		0x00000067
#define ESME_RINVOPTPARSTREAM	0x000000C0
#define ESME_ROPTPARNOTALLWD	0x000000C1
#define ESME_RINVPARLEN		0x000000C2
#define ESME_RMISSINGOPTPARAM	0x000000C3
#define ESME_RINVOPTPARAMVAL	0x000000C4
#define ESME_RDELIVERYFAILURE	0x000000FE
#define ESME_RUNKNOWNERR	0x000000FF
#define ECODES_COUNT		50

/*
 * TLV
 */
#define TLV_STRING_MAX_LEN	256
#define TLV_MIN_SIZE		4


typedef struct _tlv_t
{
	uint16 tag;
	uint16 length;
	union
	{
		uint8  i8;
		uint16 i16;
		uint32 i32;
		char s[TLV_STRING_MAX_LEN];
		char * c;
	};
} tlv_t;

#define TLV_SC_INTERFACE_VERSION	0x0210	// 528
#define TLV_MESSAGE_PAYLOAD		0x0424	// 1060
#define TLV_SAR_MSG_REF_NUM		0x020C
#define TLV_SAR_TOTAL_SEGMENTS		0x020E
#define TLV_SAR_SEGMENT_SEQNUM		0x020F
#define TLV_MAXUS_MESSAGE_ID		0x1420
#define TLV_MAXUS_REAL_MESSAGES_COUNT	0x1430

/*
 * Encodings 
 */
#define DC_UCS2BE 			0x08
#define DC_UNI				0x18 // I dont know what is it, but I saw it in other ESME source code. Let it be.

#endif
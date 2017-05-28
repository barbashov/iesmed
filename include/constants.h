#ifndef __CONSTANTS_H_
#define __CONSTANTS_H_

#include <getopt.h>

const static struct option arg_options[] = {
	{"id",1,0,'i'},
	{"debug",0,0,'d'},
	{"foreground",0,0,'f'},
	{0,0,0,0}
};

#define SZ_BUFFER_SIZE			1024
#define SECONDS_IN_DAY			86400

#define VERSION_MAJOR			0
#define VERSION_MINOR			1
#define VERSION_SUFFIX			"alpha"

#define TLV_NOVALUE		0
#define TLV_DONT_STORE		1
#define TLV_STORE_AS_INT	2
#define TLV_STORE_AS_STRING	3
const char c_tlv_store_as[4][13] = {
	"no value",
	"don't store",
	"integer",
	"string"
};
#define TLV_VALTYPE_UINT8	1
#define TLV_VALTYPE_UINT16	2
#define TLV_VALTYPE_UINT32	3
#define TLV_VALTYPE_STRING	4
const char c_tlv_value_type[5][22] = {
	"no value",
	"unsigned char",
	"unsigned short int",
	"unsigned int",
	"string"
};

#endif

#ifndef __TYPES_H_
#define __TYPES_H_

#include <sys/types.h>
//typedef unsigned int		uint32;
typedef u_int32_t		uint32;
typedef unsigned short int	uint16;
typedef unsigned char		uint8;

typedef struct _tlv_map_t
{
	uint8 store_as;
	uint8 value_type;
	char * column_name;
	char * description;
} tlv_map_t;

#endif

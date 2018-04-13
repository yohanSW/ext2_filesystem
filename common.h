/******************************************************************************/
/*                                                                            */
/* Project : EXT2 File System                                             */
/* File    : disk.h                                                           */
/* Author  : Kim yohan (kyh9369@q.ssu.ac.kr)                                   */
/* Company : soongsil Univ. network computing Lab.                            */
/* Notes   : Common macros header                                              */
/* Date    : 2017/2/10                                                         */
/*                                                                            */
/******************************************************************************/
//we fix a FAT12/16 File System. the system's index is at under

/******************************************************************************/
/*                                                                            */
/* Project : FAT12/16 File System                                             */
/* File    : common.h                                                         */
/* Author  : Kyoungmoon Sun(msg2me@msn.com)                                   */
/* Company : Dankook Univ. Embedded System Lab.                               */
/* Notes   : Common macros header                                             */
/* Date    : 2008/7/2                                                         */
/*                                                                            */
/******************************************************************************/

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "types.h"

#ifndef ZeroMemory
#define ZeroMemory( a, b )      memset( a, 0, b )
#endif

#define PRINTF					printf
#define SECTOR					DWORD

#define STRINGIFY(x)			#x
#define TOSTRING(x)				STRINGIFY(x)

#ifndef __FUNCTION__
#define __LOCATION__			__FILE__ "(" TOSTRING(__LINE__) ") "
#else
#define __LOCATION__			__FILE__ "(" TOSTRING(__LINE__) ", " __FUNCTION__ ") "
#endif

#define WARNING( ... )			PRINTF( __VA_ARGS__ )

#ifndef _DEBUG
#define TRACE
#else
#define TRACE                   PRINTF
#endif

#define STEP( a )				{ PRINTF( "%s(%d): %s;\n", __FILE__, __LINE__, # a ); a; }

#define EXT2_ERROR				-1
#define EXT2_SUCCESS				0

#endif

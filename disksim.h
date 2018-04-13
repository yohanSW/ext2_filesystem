/******************************************************************************/
/*                                                                            */
/* Project : EXT2 File System                                             */
/* File    : disk.h                                                           */
/* Author  : Kim yohan (kyh9369@q.ssu.ac.kr)                                   */
/* Company : soongsil Univ. network computing Lab.                            */
/* Notes   : Disk simulator header                                              */
/* Date    : 2017/2/10                                                         */
/*                                                                            */
/******************************************************************************/
//we fix a FAT12/16 File System. the system's index is at under

/******************************************************************************/
/*                                                                            */
/* Project : FAT12/16 File System                                             */
/* File    : disksim.h                                                        */
/* Author  : Kyoungmoon Sun(msg2me@msn.com)                                   */
/* Company : Dankook Univ. Embedded System Lab.                               */
/* Notes   : Disk simulator header                                            */
/* Date    : 2008/7/2                                                         */
/*                                                                            */
/******************************************************************************/
//disksim = 장치 시뮬레이터
#ifndef _DISKSIM_H_
#define _DISKSIM_H_

#include "common.h"
#include "disk.h"

int disksim_init( SECTOR, unsigned int, DISK_OPERATIONS* );
void disksim_uninit( DISK_OPERATIONS* );

#endif

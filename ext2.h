/******************************************************************************/
/*                                                                            */
/* Project : EXT2 File System                                             */
/* File    : disk.h                                                           */
/* Author  : Kim yohan (kyh9369@q.ssu.ac.kr)                                   */
/* Company : soongsil Univ. network computing Lab.                            */
/* Notes   : ext2.h                                          */
/* Date    : 2017/2/10                                                         */
/*                                                                            */
/******************************************************************************/


#ifndef _EXT2_H_
#define _EXT2_H_

#include "common.h"
#include "disk.h"
#include "types.h"

#define MAX_SECTOR_SIZE 			512
#define MAX_NAME_LENGTH 			256
#define MAX_ENTRY_NAME_LENGTH 		11
#define EXT2_NDIR_BLOCKS 			12
#define EXT2_IND_BLOCK 				EXT2_NDIR_BLOCKS
#define EXT2_DIND_BLOCK 			(EXT2_IND_BLOCK+1)
#define EXT2_TIND_BLOCK				(EXT2_DIND_BLOCK+1)
#define EXT2_N_BLOCKS				(EXT2_TIND_BLOCK+1)
#define EXT2_NAME_LEN				11

#define EXT2_VALID_FS				1
#define EXT2_ERROR_FS				2
#define EXT2_ERRORS_CONTINUE		1 // nothing happend
#define EXT2_ERRORS_RO				2 // remount read_only
#define EXT2_ERRORS_PANIC			3 // kernal panic
#define EXT2_GOOD_OLD_REV			0
#define EXT2_DYNAMIC_REV			1
#define EXT2_OS_LINUX				0

//#define NUMBER_OF_BLOCK_GROUP			
#define BLOCK_SIZE_IN_BYTES			4096
#define SECTORS_PER_BLOCK			(BLOCK_SIZE_IN_BYTES/MAX_SECTOR_SIZE)
#define BLOCK_PER_GROUP				(BLOCK_SIZE_IN_BYTES * 8)
#define GROUP_SIZE_IN_BYTES			(BLOCK_PER_GROUP * BLOCK_SIZE_IN_BYTES)
#define INODE_SIZE_IN_BYTES			128
#define INODES_PER_GROUP			1024

#define DIR_ENTRY_FREE			0xE5
#define DIR_ENTRY_NO_MORE		0x00

#define ATTR_READ_ONLY			0x01
#define ATTR_HIDDEN				0x02
#define ATTR_SYSTEM				0x04
#define ATTR_VOLUME_ID			0x08
#define ATTR_DIRECTORY			0x10
#define ATTR_ARCHIVE			0x20

typedef struct ext2_super_block{
	UINT32	 s_inodes_count;//total
	UINT32	 s_blocks_count;//total
	UINT32	 s_r_block_count;//X
	UINT32	 s_free_blocks_count;//total
	UINT32	 s_free_inodes_count;//total
	UINT32	 s_first_data_block;//0
	UINT32   s_log_block_size;//4
	INT32	 s_log_frag_size;//x
	UINT32	 s_blocks_per_group;
	UINT32	 s_frags_per_group;//x
	UINT32	 s_inodes_per_group;
	UINT32	 s_mtime;//x
	UINT32	 s_wtime;//x
	UINT16	 s_mnt_count;//x
	INT16	 s_max_mnt_count;//x
	UINT16	 s_magic;//EF53
	UINT16	 s_state;//EXT2_VALID_FS
	UINT16 	 s_errors;//EXT2_ERRORS_CONTINUE;
	UINT16 	 s_minor_rev_level;//0
	UINT32	 s_lastcheck;//x
	UINT32	 s_check_interval;//x
	UINT32 	 s_creator_os;//x
	UINT32	 s_rev_level;//EXT2_GOOD_OLD_REV
	UINT16	 s_def_resuid;//x
	UINT16	 s_def_resgid;//x
	UINT32	 s_first_ino;//11
	UINT16	 s_inode_size;//128
	UINT16	 s_block_group_nr;//0
	UINT32	 s_feature_compat;//x
	UINT32	 s_feature_incompat;//x
	UINT32	 s_feature_ro_compat;//x
	UINT32	 s_reserved[230];//x
} EXT2_SUPER_BLOCK;

typedef struct
{
	UINT32	 bg_block_bitmap;//bitmap block num
	UINT32	 bg_inode_bitmap;//inode block num
	UINT32	 bg_inode_table;//inode table block num
	UINT16	 bg_free_blocks_count;//free blocks count in group
	UINT16	 bg_free_inodes_count;//free inodes count in group
	UINT16	 bg_used_dirs_count;//used dirs count in group
	UINT16	 bg_pad;//x
	UINT32	 bg_reserved[3];//x
} EXT2_GROUP_DESC;

typedef struct
{
	UINT16	 i_mode; //루트디렉토리일시 ATTR_VOLUME_ID
	UINT16	 i_uid;
	UINT32	 i_size;
	UINT32	 i_atime;//x
	UINT32	 i_ctime;//x
	UINT32	 i_mtime;//x
	UINT32	 i_dtime;//xino
	UINT16	 i_gid;
	UINT16	 i_links_count;
	UINT32	 i_blocks;
	UINT32	 i_flags;
	UINT32	 i_block[EXT2_N_BLOCKS];
	UINT32	 i_version;
	UINT32	 i_file_acl;//
	UINT32	 i_dir_acl;//
	UINT32	 i_faddr;//
	UINT32	 pad[4];
} EXT2_INODE;

typedef struct{
	EXT2_SUPER_BLOCK spb;
	DISK_OPERATIONS* disk;
}EXT2_FILESYSTEM;

typedef struct
{
	UINT32	 inode;
	UINT16	 rec_len;
	UINT16	 name_len;
	char	 name[EXT2_NAME_LEN];
	BYTE	 pad[13];
} EXT2_DIR_ENTRY;

typedef struct{
    EXT2_FILESYSTEM* fs;
    EXT2_DIR_ENTRY entry;
    UINT32 inodeNum;
}EXT2_NODE;

typedef struct {
	UINT32 blockNum;
	UINT32 entryNum;
} ENTRY_LOCATION;

typedef int (*EXT2_NODE_ADD)(void*, EXT2_NODE*);

int ext2_format(DISK_OPERATIONS* disk);
int ext2_read_superblock(EXT2_FILESYSTEM* fs, EXT2_NODE* root);
int ext2_lookup(EXT2_NODE* Parent, char* entryName, ENTRY_LOCATION* retEntry);
int ext2_fill_inode(EXT2_FILESYSTEM *fs, EXT2_INODE* inode, UINT32 inodeNum);
int ext2_dump(DISK_OPERATIONS* disk, int blockGroupNum, int type, int target);
int ext2_rmdir(EXT2_NODE*  parent);//
int ext2_mkdir(EXT2_NODE* parent, const char* entryName, EXT2_NODE* retEntry);
int ext2_df(EXT2_FILESYSTEM* fs, UINT32* totalSectors, UINT32* usedSectors);
int ext2_read_dir(EXT2_NODE* dir, EXT2_NODE_ADD adder, void* list); //
int ext2_write(EXT2_NODE* file, unsigned long offset, unsigned long length, const char* buffer);
int ext2_read(EXT2_NODE* file, unsigned long offset, unsigned long length, char* buffer);
int ext2_remove(EXT2_NODE* file);
int ext2_create( EXT2_NODE* EXT2Parent, const char* name, EXT2_NODE* Entry );

#endif

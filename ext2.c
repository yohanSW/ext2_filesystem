/******************************************************************************/
/*                                                                            */
/* Project : EXT2 File System                                             */
/* File    : disk.h                                                           */
/* Author  : Kim yohan (kyh9369@q.ssu.ac.kr)                                   */
/* Company : soongsil Univ. network computing Lab.                            */
/* Notes   : ext2.c                                          */
/* Date    : 2017/2/10                                                         */
/*                                                                            */
/******************************************************************************/


#include<stdio.h>
#include "disksim.h"
#include "ext2.h"

#define MIN( a, b )               ( ( a ) < ( b ) ? ( a ) : ( b ) )
#define MAX( a, b )               ( ( a ) > ( b ) ? ( a ) : ( b ) )

int fill_sb(EXT2_SUPER_BLOCK *sb, SECTOR numberOfSectors, UINT32 bytesPerSector) {
	QWORD diskSize = numberOfSectors * bytesPerSector;
	UINT32 group_count = numberOfSectors * bytesPerSector / GROUP_SIZE_IN_BYTES;
	UINT32 descBlockCount = group_count * sizeof(EXT2_GROUP_DESC) / BLOCK_SIZE_IN_BYTES;
	if (group_count * sizeof(EXT2_GROUP_DESC) % BLOCK_SIZE_IN_BYTES != 0)
		descBlockCount++;

	ZeroMemory(sb, sizeof(EXT2_SUPER_BLOCK));

	sb->s_blocks_per_group = GROUP_SIZE_IN_BYTES / BLOCK_SIZE_IN_BYTES;
	sb->s_inodes_per_group = INODES_PER_GROUP;

	sb->s_inodes_count = sb->s_inodes_per_group * (diskSize / GROUP_SIZE_IN_BYTES);
	sb->s_blocks_count = sb->s_blocks_per_group * (diskSize / GROUP_SIZE_IN_BYTES);
	//   sb->s_r_block_count = 0;
	sb->s_first_ino = 11;
	sb->s_free_blocks_count = sb->s_blocks_count - (diskSize / GROUP_SIZE_IN_BYTES)*(sizeof(EXT2_INODE) * INODES_PER_GROUP / BLOCK_SIZE_IN_BYTES + descBlockCount + 3);
	sb->s_free_inodes_count = sb->s_inodes_count - (sb->s_first_ino - 1) * (diskSize / GROUP_SIZE_IN_BYTES);
	sb->s_first_data_block = 0;
	sb->s_log_block_size = 4;

	sb->s_magic = 0xEF53;
	sb->s_state = EXT2_VALID_FS;
	sb->s_errors = EXT2_ERRORS_CONTINUE;
	sb->s_minor_rev_level = 0;

	sb->s_rev_level = EXT2_GOOD_OLD_REV;

	sb->s_inode_size = INODE_SIZE_IN_BYTES;
	sb->s_block_group_nr = 0;


	printf("inodes count = %x\n", sb->s_inodes_count);
	printf("blocks count = %x\n", sb->s_blocks_count);

	printf("free blocks count = %x\n", sb->s_free_blocks_count);
	printf("free inodes count = %x\n", sb->s_free_inodes_count);
	printf("first data block = %x\n", sb->s_first_data_block);
	printf("blocks per group = %x\n", sb->s_blocks_per_group);

	return EXT2_SUCCESS;
}

int set_bit(int number, BYTE* sector) {
	BYTE value;
	int byte = number / 8;
	int offset = number % 8;

	switch (offset) {
	case 0:
		value = 0x80;
		break;
	case 1:
		value = 0x40;
		break;
	case 2:
		value = 0x20;
		break;
	case 3:
		value = 0x10;
		break;
	case 4:
		value = 0x8;
		break;
	case 5:
		value = 0x4;
		break;
	case 6:
		value = 0x2;
		break;
	case 7:
		value = 0x1;
		break;
	}
	sector[byte] |= value;
}

int init_bitmap(DISK_OPERATIONS* disk, EXT2_SUPER_BLOCK *sb, const EXT2_GROUP_DESC *desc) {
	BYTE sector[MAX_SECTOR_SIZE];
	UINT32 sectorNum;
	int i;
	int reserved;
	int datablock, inodeblock;

	//block bitmap
	ZeroMemory(sector, MAX_SECTOR_SIZE);

	for (i = 0; i<SECTORS_PER_BLOCK; i++) {
		disk->write_sector(disk, i + (desc->bg_block_bitmap * SECTORS_PER_BLOCK)/* 8 */, sector);
	}

	datablock = sb->s_first_data_block + desc->bg_inode_table + (sb->s_inodes_per_group * sizeof(EXT2_INODE) / BLOCK_SIZE_IN_BYTES);
	for (i = 0; i<datablock; i++)
		set_bit(i, sector);

	sectorNum = (desc->bg_block_bitmap * 8);
	disk->write_sector(disk, sectorNum, sector);

	//inode bitmap
	ZeroMemory(sector, MAX_SECTOR_SIZE);

	for (i = 0; i<SECTORS_PER_BLOCK; i++) {
		disk->write_sector(disk, i + desc->bg_inode_bitmap * SECTORS_PER_BLOCK, sector);
	}

	if (desc->bg_block_bitmap < BLOCK_PER_GROUP) {
		for (i = 0; i<sb->s_first_ino; i++)
			set_bit(i, sector);

		disk->write_sector(disk, desc->bg_inode_bitmap * SECTORS_PER_BLOCK, sector);
	}

	return 0;
}

int fill_entry(EXT2_DIR_ENTRY* entry, char* name, int inodeNum) {
	if (inodeNum < 0)
		return -1;

	if (strcmp(name, "no_more") == 0) {
		entry->name[0] = DIR_ENTRY_NO_MORE;
		return 0;
	}

	entry->inode = inodeNum;
	entry->name_len = strlen(name);
	entry->rec_len = sizeof(UINT32) + sizeof(UINT16) + sizeof(UINT16) + sizeof(char) * entry->name_len;
	memcpy(entry->name, name, entry->name_len);
}

int create_root(DISK_OPERATIONS* disk) {
	BYTE dirSector[MAX_SECTOR_SIZE];
	BYTE inodeSector[MAX_SECTOR_SIZE];
	BYTE descSector[MAX_SECTOR_SIZE];
	BYTE sbSector[MAX_SECTOR_SIZE];
	BYTE sector[MAX_SECTOR_SIZE];

	EXT2_SUPER_BLOCK *sb;
	EXT2_GROUP_DESC *desc;
	EXT2_DIR_ENTRY *entry;
	EXT2_INODE *inode;

	int rootInode = 2;
	int i = 0;

	EXT2_DIR_ENTRY *dotEntry;
	EXT2_DIR_ENTRY *dotdotEntry;

	UINT32 group_count = disk->numberOfSectors * disk->bytesPerSector / GROUP_SIZE_IN_BYTES;

	SECTOR rootSector = 0;

	disk->read_sector(disk, 0, sbSector);
	sb = (EXT2_SUPER_BLOCK*)sbSector;

	disk->read_sector(disk, ((sb->s_first_data_block + 1) * SECTORS_PER_BLOCK), descSector);
	desc = (EXT2_GROUP_DESC*)descSector;

	ZeroMemory(inodeSector, MAX_SECTOR_SIZE);
	inode = (EXT2_INODE*)inodeSector;
	inode += rootInode;

	inode->i_mode = ATTR_VOLUME_ID;
	inode->i_blocks = 1;
	inode->i_block[0] = desc->bg_inode_table + (sb->s_inodes_per_group * sizeof(EXT2_INODE) / BLOCK_SIZE_IN_BYTES);

	rootSector = desc->bg_inode_table * SECTORS_PER_BLOCK;
	disk->write_sector(disk, rootSector, inodeSector);

	ZeroMemory(dirSector, MAX_SECTOR_SIZE);
	entry = (EXT2_DIR_ENTRY*)dirSector;

	fill_entry(entry, "/", 2);//root_dir_entry
	entry++;
	fill_entry(entry, ".", 2);//.
	entry++;
	fill_entry(entry, "..", 2);//..
	entry++;
	fill_entry(entry, "no_more", 2);//NO_MORE

	UINT32   datablock = sb->s_first_data_block + desc->bg_inode_table + (sb->s_inodes_per_group * sizeof(EXT2_INODE) / BLOCK_SIZE_IN_BYTES);
	disk->write_sector(disk, datablock * SECTORS_PER_BLOCK, dirSector);

	//modify super block
	sb->s_free_blocks_count--;

	for (i = 0; i<group_count; i++)
		disk->write_sector(disk, sb->s_first_data_block * SECTORS_PER_BLOCK + i * BLOCK_PER_GROUP * SECTORS_PER_BLOCK, sb);

	//modify group desc
	desc->bg_free_blocks_count--;
	desc->bg_used_dirs_count++;

	//write group desc sector
	for (i = 0; i<group_count; i++)
		disk->write_sector(disk, (sb->s_first_data_block + 1) * SECTORS_PER_BLOCK + i * BLOCK_PER_GROUP * SECTORS_PER_BLOCK, descSector);


	//Datablock bitmap
	UINT32 sectorNum = datablock / (8 * MAX_SECTOR_SIZE);
	disk->read_sector(disk, (desc->bg_block_bitmap * SECTORS_PER_BLOCK) + sectorNum, sector);

	//   printf("----------%d----------\n", (desc->bg_block_bitmap * SECTORS_PER_BLOCK) + sectorNum);
	//   printf("%d\n%d\n%d\n", desc->bg_block_bitmap, SECTORS_PER_BLOCK, sectorNum);

	//   for(i=0;i<20;i++)
	//      printf("%x ", sector[i]);
	//   printf("\n");
	set_bit(datablock, sector);
	//   for(i=0;i<20;i++)
	//      printf("%x ", sector[i]);
	//   printf("\n");
	disk->write_sector(disk, desc->bg_block_bitmap * SECTORS_PER_BLOCK + sectorNum, sector);

	return 0;
}

int fill_desc(DISK_OPERATIONS *disk, EXT2_SUPER_BLOCK *sb, EXT2_GROUP_DESC *entry, UINT32 groupNum) {
	UINT32 group_count = disk->numberOfSectors * disk->bytesPerSector / GROUP_SIZE_IN_BYTES;
	UINT32 descBlockCount = group_count * sizeof(EXT2_GROUP_DESC) / BLOCK_SIZE_IN_BYTES;
	if (group_count * sizeof(EXT2_GROUP_DESC) % BLOCK_SIZE_IN_BYTES != 0)
		descBlockCount++;

	ZeroMemory(entry, sizeof(EXT2_GROUP_DESC));
	entry->bg_block_bitmap = sb->s_first_data_block + 1 + descBlockCount;
	entry->bg_inode_bitmap = entry->bg_block_bitmap + 1;
	entry->bg_inode_table = entry->bg_inode_bitmap + 1;
	entry->bg_free_blocks_count = sb->s_free_blocks_count / group_count;
	entry->bg_free_inodes_count = sb->s_free_inodes_count / group_count;
	entry->bg_used_dirs_count = 0;

	printf("group Number = %d\n", groupNum);
	printf("descBlockCount = %d\n", descBlockCount);
	printf("block_bitmap block = %d\n", entry->bg_block_bitmap);
	printf("inode_bitmap block = %d\n", entry->bg_inode_bitmap);
	printf("inode_table block = %d\n", entry->bg_inode_table);
	printf("bg_free_blocks_count = %d\n", entry->bg_free_blocks_count);
	printf("bg_free_inode_count = %d\n", entry->bg_free_inodes_count);
	printf("bg_used_dirs_count = %d\n", entry->bg_used_dirs_count);

	return EXT2_SUCCESS;
}

int ext2_format(DISK_OPERATIONS* disk) {
	EXT2_SUPER_BLOCK sb;
	BYTE sector[MAX_SECTOR_SIZE];
	EXT2_GROUP_DESC *desc;
	UINT32 i, j;
	UINT32 group_count = disk->numberOfSectors * disk->bytesPerSector / GROUP_SIZE_IN_BYTES;
	UINT32 descSector = SECTORS_PER_BLOCK;

	if (fill_sb(&sb, disk->numberOfSectors, disk->bytesPerSector) != EXT2_SUCCESS)
		return EXT2_ERROR;

	disk->write_sector(disk, 0, &sb);
	ZeroMemory(sector, MAX_SECTOR_SIZE);
	disk->write_sector(disk, 1, sector);

	//   printf("fill_sb finish\n");

	for (i = 0; i<group_count; i++) {
		if (i % (disk->bytesPerSector / sizeof(EXT2_GROUP_DESC)) == 0) {
			//         printf("desc sector zero !!!\n");
			ZeroMemory(sector, MAX_SECTOR_SIZE);
			for (j = 0; j<SECTORS_PER_BLOCK; j++)
				disk->write_sector(disk, descSector + j, sector);
			desc = (EXT2_GROUP_DESC *)sector;
		}

		if (fill_desc(disk, &sb, desc, i) != EXT2_SUCCESS)
			return EXT2_ERROR;

		if (i / (disk->bytesPerSector / sizeof(EXT2_GROUP_DESC)) != 0 || i == group_count - 1) {
			for (j = 0; j<group_count; j++) {
				disk->write_sector(disk, j * (sb.s_blocks_per_group) * SECTORS_PER_BLOCK + descSector + (i / (disk->bytesPerSector / sizeof(EXT2_GROUP_DESC))), sector);
			}
		}
		init_bitmap(disk, &sb, desc);
		desc++;
	}

	create_root(disk);

	return EXT2_SUCCESS;
}

int read_desc(EXT2_FILESYSTEM *fs, EXT2_GROUP_DESC *desc, UINT32 groupNum) {
	BYTE sector[MAX_SECTOR_SIZE];
	EXT2_GROUP_DESC *entry;


	fs->disk->read_sector(fs->disk, (fs->spb.s_first_data_block + 1)*SECTORS_PER_BLOCK, sector);
	entry = (EXT2_GROUP_DESC*)sector;
	entry += groupNum;

	desc->bg_block_bitmap = entry->bg_block_bitmap;
	desc->bg_free_blocks_count = entry->bg_free_blocks_count;
	desc->bg_free_inodes_count = entry->bg_free_inodes_count;
	desc->bg_inode_bitmap = entry->bg_inode_bitmap;
	desc->bg_inode_table = entry->bg_inode_table;
	desc->bg_used_dirs_count = entry->bg_used_dirs_count;

	return EXT2_SUCCESS;
}

int ext2_read_superblock(EXT2_FILESYSTEM* fs, EXT2_NODE* root) {
	BYTE   sector[MAX_SECTOR_SIZE];
	UINT32 datablock;
	EXT2_GROUP_DESC desc;

	if (fs == NULL || fs->disk == NULL) {
		WARNING(" DISK OPERATIONS : %p \n FAT_FILESYSTEM : %p \n", fs, fs->disk);
		return EXT2_ERROR;
	}

	if (fs->disk->read_sector(fs->disk, 0, &fs->spb))
		return EXT2_ERROR;
	read_desc(fs, &desc, 0);
	datablock = fs->spb.s_first_data_block + desc.bg_inode_table + (fs->spb.s_inodes_per_group * sizeof(EXT2_INODE) / BLOCK_SIZE_IN_BYTES);
	fs->disk->read_sector(fs->disk, datablock * SECTORS_PER_BLOCK, sector);

	ZeroMemory(root, sizeof(EXT2_NODE));
	memcpy(&root->entry, sector, sizeof(EXT2_DIR_ENTRY));
	root->fs = fs;

	memset(root->entry.name, 0x20, 11);

	root->inodeNum = 2; // root inode

	printf("root -> entry.name : %s \n", root->entry.name);
	printf("root -> inode : %d \n", root->inodeNum);

	return EXT2_SUCCESS;
} //minji2.c

int alloc_free_block(EXT2_NODE* parent) { //반환:아이노드
	int free_groupNum = parent->entry.inode / INODES_PER_GROUP;
	int i, j, k;
	int offset;
	int blockNum;
	EXT2_GROUP_DESC* desc = (EXT2_GROUP_DESC*)malloc(sizeof(EXT2_GROUP_DESC));
	UINT32 group_count = parent->fs->disk->numberOfSectors * parent->fs->disk->bytesPerSector / GROUP_SIZE_IN_BYTES;
	BYTE sector[MAX_SECTOR_SIZE];

	read_desc(parent->fs, desc, free_groupNum);

	for (i = 0; i < SECTORS_PER_BLOCK; i++) {
		parent->fs->disk->read_sector(parent->fs->disk, i + desc->bg_block_bitmap * SECTORS_PER_BLOCK, sector);
		for (j = 0; j < MAX_SECTOR_SIZE; j++) {
			if (sector[j] == 0xFF)
				continue;
			if (sector[j] >= 0xF0) {
				switch (sector[j] % 0xF0) {
				case 0x0:
					offset = 4;
					break;
				case 0x8:
					offset = 5;
					break;
				case 0xC:
					offset = 6;
					break;
				case 0xE:
					offset = 7;
				}
			}
			else {
				switch (sector[j] / 0x10) {
				case 0x0:
					offset = 0;
					break;
				case 0x8:
					offset = 1;
					break;
				case 0xC:
					offset = 2;
					break;
				case 0xE:
					offset = 3;
				}
			}
			blockNum = (i*MAX_SECTOR_SIZE + j) * 8 + offset;
			set_bit(blockNum, sector);
			parent->fs->disk->write_sector(parent->fs->disk, i + desc->bg_block_bitmap * SECTORS_PER_BLOCK, sector);
			//슈퍼블록
			parent->fs->spb.s_free_blocks_count--;

			for (j = 0; i<group_count; i++)
				parent->fs->disk->write_sector(parent->fs->disk, parent->fs->spb.s_first_data_block * SECTORS_PER_BLOCK + i * BLOCK_PER_GROUP * SECTORS_PER_BLOCK, &parent->fs->spb);
			//descriptor

			parent->fs->disk->read_sector(parent->fs->disk, (parent->fs->spb.s_first_data_block + 1) * SECTORS_PER_BLOCK, sector);
			desc = (EXT2_GROUP_DESC*)sector;
			desc += free_groupNum;

			desc->bg_free_blocks_count--;

			//write group desc sector
			for (j = 0; j<group_count; j++)
				parent->fs->disk->write_sector(parent->fs->disk, (parent->fs->spb.s_first_data_block + 1) * SECTORS_PER_BLOCK + j * BLOCK_PER_GROUP * SECTORS_PER_BLOCK, sector);
			return blockNum;
		}
	}
	return EXT2_ERROR;
}

int get_datablock(EXT2_NODE* file, EXT2_INODE *inode, unsigned long offset, BYTE *sector) {
	DWORD blockOffset = offset / BLOCK_SIZE_IN_BYTES, blockOffsetCnt;
	UINT32 *currentBlock;


	currentBlock = inode->i_block;
	if (blockOffset / BLOCK_SIZE_IN_BYTES <= EXT2_IND_BLOCK)
		currentBlock += blockOffset;
	else if (blockOffset / BLOCK_SIZE_IN_BYTES <= EXT2_IND_BLOCK + BLOCK_SIZE_IN_BYTES / 4) {
		blockOffsetCnt = offset - EXT2_NDIR_BLOCKS;
		currentBlock += EXT2_DIND_BLOCK;

		if (*currentBlock == NULL)
			*currentBlock = alloc_free_block(file);
		file->fs->disk->read_sector(file->fs->disk, *currentBlock*SECTORS_PER_BLOCK + blockOffsetCnt / (MAX_SECTOR_SIZE / 4), sector);
		blockOffsetCnt %= (MAX_SECTOR_SIZE / 4);
		currentBlock = &((UINT32 *)sector)[blockOffsetCnt];
	}
	else if (blockOffset / BLOCK_SIZE_IN_BYTES <= EXT2_IND_BLOCK + BLOCK_SIZE_IN_BYTES / 4 + BLOCK_SIZE_IN_BYTES / 4 * BLOCK_SIZE_IN_BYTES / 4) {
		blockOffsetCnt = offset - EXT2_NDIR_BLOCKS - BLOCK_SIZE_IN_BYTES / 4;
		currentBlock += EXT2_TIND_BLOCK;

		if (*currentBlock == NULL)
			*currentBlock = alloc_free_block(file);
		file->fs->disk->read_sector(file->fs->disk, *currentBlock*SECTORS_PER_BLOCK + blockOffsetCnt / ((BLOCK_SIZE_IN_BYTES / 4)* (MAX_SECTOR_SIZE / 4)), sector);
		blockOffsetCnt %= ((BLOCK_SIZE_IN_BYTES / 4)* (MAX_SECTOR_SIZE / 4));
		currentBlock = &((UINT32 *)sector)[blockOffsetCnt];

		if (*currentBlock == NULL)
			*currentBlock = alloc_free_block(file);
		file->fs->disk->read_sector(file->fs->disk, *currentBlock*SECTORS_PER_BLOCK + blockOffsetCnt / (MAX_SECTOR_SIZE / 4), sector);
		blockOffsetCnt %= (MAX_SECTOR_SIZE / 4);
		currentBlock = &((UINT32 *)sector)[blockOffsetCnt];
	}
	else {
		blockOffsetCnt = offset - EXT2_NDIR_BLOCKS - BLOCK_SIZE_IN_BYTES / 4 - (BLOCK_SIZE_IN_BYTES / 4) * (BLOCK_SIZE_IN_BYTES / 4);
		currentBlock += EXT2_N_BLOCKS;

		if (*currentBlock == NULL)
			*currentBlock = alloc_free_block(file);
		file->fs->disk->read_sector(file->fs->disk, *currentBlock*SECTORS_PER_BLOCK + blockOffsetCnt / ((BLOCK_SIZE_IN_BYTES / 4)* (MAX_SECTOR_SIZE / 4)* (MAX_SECTOR_SIZE / 4)), sector);
		blockOffsetCnt %= ((BLOCK_SIZE_IN_BYTES / 4)* (MAX_SECTOR_SIZE / 4)* (MAX_SECTOR_SIZE / 4));
		currentBlock = &((UINT32 *)sector)[blockOffsetCnt];

		if (*currentBlock == NULL)
			*currentBlock = alloc_free_block(file);
		file->fs->disk->read_sector(file->fs->disk, *currentBlock*SECTORS_PER_BLOCK + blockOffsetCnt / ((BLOCK_SIZE_IN_BYTES / 4)* (MAX_SECTOR_SIZE / 4)), sector);
		blockOffsetCnt %= ((BLOCK_SIZE_IN_BYTES / 4)* (MAX_SECTOR_SIZE / 4));
		currentBlock = &((UINT32 *)sector)[blockOffsetCnt];

		if (*currentBlock == NULL)
			*currentBlock = alloc_free_block(file);
		file->fs->disk->read_sector(file->fs->disk, *currentBlock*SECTORS_PER_BLOCK + blockOffsetCnt / (MAX_SECTOR_SIZE / 4), sector);
		blockOffsetCnt %= (MAX_SECTOR_SIZE / 4);
		currentBlock = &((UINT32 *)sector)[blockOffsetCnt];
	}
	file->fs->disk->read_sector(file->fs->disk, *currentBlock*SECTORS_PER_BLOCK + (offset / MAX_SECTOR_SIZE), sector);
	return (int)*currentBlock;
}

int ext2_lookup(EXT2_NODE* parent, char* entryName, ENTRY_LOCATION *location) {//block번호와 몇번째 엔트리인지 리턴
	EXT2_INODE *inode = (EXT2_INODE*)malloc(sizeof(EXT2_INODE));
	EXT2_DIR_ENTRY *entry;
	BYTE sector[MAX_SECTOR_SIZE];
	UINT32 i, j, k;
	int blockNum;
	//부모 inode읽어
	ext2_fill_inode(parent->fs, inode, parent->entry.inode);

	for (i = 0; i < inode->i_blocks; i++) {
		for (j = 0; j < SECTORS_PER_BLOCK; j++) {
			blockNum = get_datablock(parent, inode, (i*SECTORS_PER_BLOCK + j)*MAX_SECTOR_SIZE, sector);
			entry = (EXT2_DIR_ENTRY*)sector;

			for (k = 0; k < MAX_SECTOR_SIZE / sizeof(EXT2_DIR_ENTRY); k++) {
				if (entryName[0] == DIR_ENTRY_NO_MORE) {
					if (entry->name[0] == DIR_ENTRY_NO_MORE) {
						location->blockNum = blockNum;
						location->entryNum = j*MAX_SECTOR_SIZE / sizeof(EXT2_DIR_ENTRY) + k;
						return EXT2_SUCCESS;
					}
				}
				else if (entry->name[0] == DIR_ENTRY_NO_MORE) {
					return EXT2_ERROR;
				}
				else {
					if (strcmp(entry->name, entryName) == 0) {
						location->blockNum = blockNum;
						location->entryNum = j*MAX_SECTOR_SIZE / sizeof(EXT2_DIR_ENTRY) + k;
						return EXT2_SUCCESS;
					}
				}
				entry++;
			}
		}
	}
}

int ext2_fill_inode(EXT2_FILESYSTEM *fs, EXT2_INODE* inode, UINT32 inodeNum) {
	if (inodeNum < 0)
		return EXT2_ERROR;

	BYTE sector[MAX_SECTOR_SIZE];
	EXT2_INODE *entry;
	EXT2_GROUP_DESC desc;

	UINT32 i, groupNum = inodeNum / INODES_PER_GROUP;
	read_desc(fs, &desc, groupNum);

	fs->disk->read_sector(fs->disk, (groupNum*BLOCK_PER_GROUP + desc.bg_inode_table)*SECTORS_PER_BLOCK, sector);
	entry = (EXT2_INODE*)sector;
	entry += inodeNum%INODES_PER_GROUP;

	inode->i_mode = entry->i_mode;
	inode->i_blocks = entry->i_blocks;

	for (i = 0; i<EXT2_NDIR_BLOCKS; i++) {
		inode->i_block[i] = entry->i_block[i];

		if (i + 1 == entry->i_blocks)
			return EXT2_SUCCESS;
	}

	inode->i_block[EXT2_IND_BLOCK] = entry->i_block[EXT2_IND_BLOCK];
	if (inode->i_blocks <= EXT2_NDIR_BLOCKS + BLOCK_SIZE_IN_BYTES / sizeof(UINT32))
		return EXT2_SUCCESS;

	inode->i_block[EXT2_DIND_BLOCK] = entry->i_block[EXT2_DIND_BLOCK];
	if (inode->i_blocks <= EXT2_NDIR_BLOCKS + BLOCK_SIZE_IN_BYTES / sizeof(UINT32) + BLOCK_SIZE_IN_BYTES / sizeof(UINT32)*BLOCK_SIZE_IN_BYTES / sizeof(UINT32))
		return EXT2_SUCCESS;

	inode->i_block[EXT2_TIND_BLOCK] = entry->i_block[EXT2_TIND_BLOCK];

	return EXT2_SUCCESS;
}

int ext2_dump(DISK_OPERATIONS* disk, int blockGroupNum, int type, int target) {
	int i, j, blockNum, sectorCnt;
	BYTE blockSector[MAX_SECTOR_SIZE];
	UINT32 group_count = disk->numberOfSectors * disk->bytesPerSector / GROUP_SIZE_IN_BYTES;
	UINT32 descBlockCount = group_count * sizeof(EXT2_GROUP_DESC) / BLOCK_SIZE_IN_BYTES;
	if (group_count * sizeof(EXT2_GROUP_DESC) % BLOCK_SIZE_IN_BYTES != 0)
		descBlockCount++;
	UINT32 inodeBlockCount = (INODES_PER_GROUP * INODE_SIZE_IN_BYTES) / BLOCK_SIZE_IN_BYTES;

	switch (type) {
	case 1:
		blockNum = 0;
		sectorCnt = 2;
		break;
	case 2:
		blockNum = 1;
		sectorCnt = descBlockCount * SECTORS_PER_BLOCK;
		break;
	case 3:
		blockNum = BLOCK_PER_GROUP * blockGroupNum + 1 + descBlockCount;
		sectorCnt = SECTORS_PER_BLOCK;
		break;
	case 4:
		blockNum = BLOCK_PER_GROUP * blockGroupNum + 2 + descBlockCount;
		sectorCnt = SECTORS_PER_BLOCK;
		break;
	case 5:
		blockNum = BLOCK_PER_GROUP * blockGroupNum + 3 + descBlockCount;
		sectorCnt = inodeBlockCount * SECTORS_PER_BLOCK;
		break;
	case 6:
		blockNum = target;
		sectorCnt = SECTORS_PER_BLOCK;
		break;
	}

	for (i = 0; i<sectorCnt; i++) {
		disk->read_sector(disk, blockNum * SECTORS_PER_BLOCK + i, blockSector);
		if (i == 0)
			printf("start adress : %p , end address : %p\n\n", blockSector, blockSector + MAX_SECTOR_SIZE*sectorCnt);
		for (j = 0; j<MAX_SECTOR_SIZE; j++) {

			if (j % 16 == 0)
				printf("\n%p\t ", &blockSector[j]);

			if (blockSector[j] <= 0xf)
				printf(" 0%x ", blockSector[j]);
			else
				printf(" %x ", blockSector[j]);
		}
	}
	printf("\n\n");

	return 0;
}

/////////////////////////////
int ext2_rmdir(EXT2_NODE* dir) {
	return 0;
}

void upper_string(char* str, int length)
{
	while (*str && length-- > 0)
	{
		*str = toupper(*str);
		str++;
	}
}

int format_name(EXT2_FILESYSTEM* fs, char* name) {
	UINT32   i, length;
	UINT32   extender = 0, nameLength = 0;
	UINT32   extenderCurrent = 8;
	BYTE   regularName[MAX_ENTRY_NAME_LENGTH];

	memset(regularName, 0x20, sizeof(regularName));
	length = strlen(name);

	if (strncmp(name, "..", 2) == 0)
	{
		memcpy(name, "..         ", 11);
		return EXT2_SUCCESS;
	}
	else if (strncmp(name, ".", 1) == 0)
	{
		memcpy(name, ".          ", 11);
		return EXT2_SUCCESS;
	}

	upper_string(name, MAX_ENTRY_NAME_LENGTH);

	for (i = 0; i < length; i++)
	{
		if (name[i] != '.' && !isdigit(name[i]) && !isalpha(name[i]))
			return EXT2_ERROR;

		if (name[i] == '.')
		{
			if (extender)
				return EXT2_ERROR;      /* dot character is allowed only once */
			extender = 1;
		}
		else if (isdigit(name[i]) || isalpha(name[i]))
		{
			if (extender)
				regularName[extenderCurrent++] = name[i];
			else
				regularName[nameLength++] = name[i];
		}
		else
			return EXT2_ERROR;         /* non-ascii name is not allowed */
	}

	if (nameLength > 8 || nameLength == 0 || extenderCurrent > 11)
		return EXT2_ERROR;

	memcpy(name, regularName, sizeof(regularName));
	return EXT2_SUCCESS;
}

int insert_inode(EXT2_NODE *retEntry, UINT32 inodeNum, UINT16 mode) {
	if (inodeNum < 0)
		return EXT2_ERROR;

	UINT32 inodeGroup, inodeBlock, inodeSector, inodeOffset;
	UINT32 blocks, datablock;
	BYTE sector[MAX_SECTOR_SIZE];
	EXT2_GROUP_DESC *entry = (EXT2_GROUP_DESC*)malloc(sizeof(EXT2_GROUP_DESC));
	EXT2_INODE *inode;
	DISK_OPERATIONS *disk = retEntry->fs->disk;

	inodeGroup = inodeNum / INODES_PER_GROUP;
	inodeBlock = inodeNum%INODES_PER_GROUP * sizeof(EXT2_INODE) / BLOCK_SIZE_IN_BYTES;
	inodeSector = inodeNum%INODES_PER_GROUP * sizeof(EXT2_INODE) / MAX_SECTOR_SIZE;
	inodeOffset = (inodeNum%INODES_PER_GROUP) % (MAX_SECTOR_SIZE / sizeof(EXT2_INODE));

	read_desc(retEntry->fs, entry, inodeGroup);

	disk->read_sector(disk, (entry->bg_inode_table + inodeGroup*BLOCK_PER_GROUP + inodeBlock)*SECTORS_PER_BLOCK + inodeSector, sector);
	inode = (EXT2_INODE*)sector;
	inode += inodeOffset;

	if (mode == ATTR_ARCHIVE) {
		blocks = 0;

		inode->i_mode = ATTR_ARCHIVE;
		inode->i_blocks = blocks;

		disk->write_sector(disk, (entry->bg_inode_table + inodeGroup*BLOCK_PER_GROUP + inodeBlock)*SECTORS_PER_BLOCK + inodeSector, sector);
		return EXT2_SUCCESS;
	}

	blocks = 1;

	inode->i_mode = ATTR_DIRECTORY;
	inode->i_blocks = blocks;

	datablock = alloc_free_block(retEntry);///////////////////
	if (datablock == EXT2_ERROR)
		return EXT2_ERROR;
	inode->i_block[0] = datablock;

	disk->write_sector(disk, (entry->bg_inode_table + inodeGroup*BLOCK_PER_GROUP + inodeBlock)*SECTORS_PER_BLOCK + inodeSector, sector);

	return datablock;
}

int find_free_group(EXT2_NODE* parent) { // 파일을 생성 할 적절한 그룹 찾기
	EXT2_GROUP_DESC desc;
	int parent_group = parent->inodeNum / INODES_PER_GROUP;
	int free_group = parent_group, i;
	int groupSum = (parent->fs->disk->numberOfSectors * parent->fs->disk->bytesPerSector) / GROUP_SIZE_IN_BYTES;
	int desc_sector = 0;

	read_desc(parent->fs, &desc, parent_group);

	if (desc.bg_free_blocks_count && desc.bg_free_inodes_count) //부모가 있는 그룹에 free가 있다면 반환
		return free_group;

	for (i = 0; i < groupSum; i++) {
		if (++free_group >= groupSum)
			free_group = 0;
		read_desc(parent, &desc, free_group);
		if (desc.bg_free_blocks_count && desc.bg_free_inodes_count)
			return free_group;
		else
			free_group++;
	}
	return EXT2_ERROR;
}

int alloc_free_inode(EXT2_NODE* parent) { //반환:아이노드
	int free_groupNum = find_free_group(parent);
	int i, j, k;
	int offset;
	int inodeNum;
	EXT2_GROUP_DESC* desc = (EXT2_GROUP_DESC*)malloc(sizeof(EXT2_GROUP_DESC));
	UINT32 group_count = parent->fs->disk->numberOfSectors * parent->fs->disk->bytesPerSector / GROUP_SIZE_IN_BYTES;
	BYTE sector[MAX_SECTOR_SIZE];

	read_desc(parent->fs, desc, free_groupNum);

	for (i = 0; i < SECTORS_PER_BLOCK; i++) {
		parent->fs->disk->read_sector(parent->fs->disk, i + desc->bg_inode_bitmap * SECTORS_PER_BLOCK, sector);
		for (j = 0; j < MAX_SECTOR_SIZE; j++) {
			if (sector[j] == 0xFF)
				continue;
			if (sector[j] >= 0xF0) {
				switch (sector[j] % 0xF0) {
				case 0x0:
					offset = 4;
					break;
				case 0x8:
					offset = 5;
					break;
				case 0xC:
					offset = 6;
					break;
				case 0xE:
					offset = 7;
				}
			}
			else {
				switch (sector[j] / 0x10) {
				case 0x0:
					offset = 0;
					break;
				case 0x8:
					offset = 1;
					break;
				case 0xC:
					offset = 2;
					break;
				case 0xE:
					offset = 3;
				}
				inodeNum = j * 8 + offset;
				set_bit(inodeNum, sector);
				parent->fs->disk->write_sector(parent->fs->disk, i + desc->bg_inode_bitmap * SECTORS_PER_BLOCK, sector);
				//슈퍼블록
				parent->fs->spb.s_free_inodes_count--;

				for (j = 0; i<group_count; i++)
					parent->fs->disk->write_sector(parent->fs->disk, parent->fs->spb.s_first_data_block * SECTORS_PER_BLOCK + i * BLOCK_PER_GROUP * SECTORS_PER_BLOCK, &parent->fs->spb);
				//descriptor

				parent->fs->disk->read_sector(parent->fs->disk, (parent->fs->spb.s_first_data_block + 1) * SECTORS_PER_BLOCK, sector);
				desc = (EXT2_GROUP_DESC*)sector;
				desc += free_groupNum;

				desc->bg_free_inodes_count--;

				//write group desc sector
				for (j = 0; j<group_count; j++)
					parent->fs->disk->write_sector(parent->fs->disk, (parent->fs->spb.s_first_data_block + 1) * SECTORS_PER_BLOCK + j * BLOCK_PER_GROUP * SECTORS_PER_BLOCK, sector);
				return inodeNum;
			}
		}
	}
	return EXT2_ERROR;
}

int ext2_mkdir(EXT2_NODE* parent, const char* entryName, EXT2_NODE* retEntry) {
	EXT2_INODE inode;
	EXT2_NODE dotNode, dotdotNode;
	ENTRY_LOCATION location;
	EXT2_DIR_ENTRY *entry;
	EXT2_GROUP_DESC *desc;

	BYTE name[MAX_NAME_LENGTH];
	BYTE sector[MAX_SECTOR_SIZE];
	int result, i;
	UINT32 sectorOffset;
	UINT32 entryOffset, datablock;

	strncpy(name, entryName, MAX_NAME_LENGTH);

	if (format_name(parent->fs, name))
		return EXT2_ERROR;

	if (ext2_lookup(parent, name, &location) == EXT2_SUCCESS)
		return EXT2_ERROR;

	//new Entry
	ZeroMemory(retEntry, sizeof(EXT2_NODE));
	fill_entry(&retEntry->entry, name, alloc_free_inode(parent));
	if (retEntry->entry.inode == EXT2_ERROR) {
		return EXT2_ERROR;
	}
	ext2_fill_inode(parent->fs, &inode, retEntry->entry.inode);

	name[0] = DIR_ENTRY_NO_MORE;
	for (i = 0; i < SECTORS_PER_BLOCK; i++) {
		if (ext2_lookup(parent, name, &location) == EXT2_SUCCESS)
			break;
	}

	sectorOffset = location.entryNum * sizeof(EXT2_DIR_ENTRY) / MAX_SECTOR_SIZE;
	entryOffset = location.entryNum * sizeof(EXT2_DIR_ENTRY) % MAX_SECTOR_SIZE / sizeof(EXT2_DIR_ENTRY);

	parent->fs->disk->read_sector(parent->fs->disk, sectorOffset + location.blockNum*SECTORS_PER_BLOCK, sector);
	entry = (EXT2_DIR_ENTRY*)sector;
	entry += entryOffset;

	fill_entry(entry, retEntry->entry.name, retEntry->entry.inode);
	parent->fs->disk->write_sector(parent->fs->disk, sectorOffset + location.blockNum*SECTORS_PER_BLOCK, sector);

	retEntry->fs = parent->fs;

	//no more entry 추가
	//	if (++entryOffset >= MAX_SECTOR_SIZE / sizeof(EXT2_DIR_ENTRY);

	datablock = insert_inode(retEntry, retEntry->entry.inode, ATTR_DIRECTORY);

	ZeroMemory(sector, MAX_SECTOR_SIZE);
	entry = (EXT2_DIR_ENTRY*)sector;

	fill_entry(entry, ".", retEntry->entry.inode);//.
	entry++;
	fill_entry(entry, "..", parent->entry.inode);//..
	entry++;
	fill_entry(entry, "no_more", retEntry->entry.inode);//NO_MORE

	parent->fs->disk->write_sector(parent->fs->disk, (retEntry->entry.inode / INODES_PER_GROUP*BLOCK_PER_GROUP + datablock) * SECTORS_PER_BLOCK, sector);

	//descriptor 읽어와서 bg_used_dirs_count ++ 하고 다시 쓰기
	parent->fs->disk->read_sector(parent->fs->disk, ((parent->fs->spb.s_first_data_block + 1) * SECTORS_PER_BLOCK), sector);
	desc = (EXT2_GROUP_DESC*)sector;
	desc += retEntry->entry.inode / INODES_PER_GROUP;

	desc->bg_used_dirs_count++;

	for (i = 0; i < parent->fs->disk->numberOfSectors * parent->fs->disk->bytesPerSector / GROUP_SIZE_IN_BYTES; i++)
		parent->fs->disk->write_sector(parent->fs->disk, (parent->fs->spb.s_first_data_block + 1) * SECTORS_PER_BLOCK + i * BLOCK_PER_GROUP * SECTORS_PER_BLOCK, sector);

	return EXT2_SUCCESS;
}

int ext2_df(EXT2_FILESYSTEM* fs, UINT32* totalSectors, UINT32* usedSectors) {
	if ((fs->spb.s_blocks_count) != 0);
	else
		*totalSectors = fs->spb.s_free_blocks_count * 8;

	*usedSectors = (fs->spb.s_blocks_count - fs->spb.s_free_blocks_count) * 8;

	return EXT2_SUCCESS;
}//disk free // minji3.c

////////////////////////////////////////////////////////////////////
int ext2_read_dir(EXT2_NODE *entry, EXT2_NODE_ADD adder, void* list) {
	return 0;
}

int ext2_write(EXT2_NODE* file, unsigned long offset, unsigned long length, const char* buffer) {
	BYTE sector[MAX_SECTOR_SIZE];
	int i;
	EXT2_INODE inode;
	DWORD readEnd;
	DWORD blockOffset = offset / BLOCK_SIZE_IN_BYTES, blockOffsetCnt;

	ext2_fill_inode(file->fs, &inode, file->entry.inode);
	offset = inode.i_size + 1;

	get_datablock(file, &inode, offset, sector);
	while (length != 0)
	{
		if (MAX_SECTOR_SIZE - offset% MAX_SECTOR_SIZE < length) {
			memcpy(&sector[offset% MAX_SECTOR_SIZE], buffer, length);
			inode.i_size += length;
			length = 0;
		}
		else {
			memcpy(&sector[offset% MAX_SECTOR_SIZE], buffer, MAX_SECTOR_SIZE - offset% MAX_SECTOR_SIZE);
			inode.i_size += MAX_SECTOR_SIZE - offset% MAX_SECTOR_SIZE;
			buffer += MAX_SECTOR_SIZE - offset% MAX_SECTOR_SIZE;
			length -= MAX_SECTOR_SIZE - offset% MAX_SECTOR_SIZE;
			offset += MAX_SECTOR_SIZE - offset% MAX_SECTOR_SIZE;
			get_datablock(file, &inode, offset, sector);
		}
	}
	return 0;
}

int ext2_read(EXT2_NODE* file, unsigned long offset, unsigned long length, char* buffer) {
	BYTE sector[MAX_SECTOR_SIZE];
	int i;
	EXT2_INODE inode;
	DWORD readEnd;
	DWORD blockOffset = offset / BLOCK_SIZE_IN_BYTES, blockOffsetCnt;
	UINT32 *currentBlock;
	UINT32 sectorOffset;

	ext2_fill_inode(file->fs, &inode, file->entry.inode);
	readEnd = MIN(offset + length, inode.i_size);
	if (readEnd <= offset)
		return EXT2_ERROR;
	else if (readEnd == inode.i_size)
		length = inode.i_size - offset;

	sectorOffset = get_datablock(file, &inode, offset, sector)*SECTORS_PER_BLOCK + (offset / MAX_SECTOR_SIZE);
	for (i = 0; i < length; i++) {
		offset += i;
		if (offset %= MAX_SECTOR_SIZE == 0) {
			if (++sectorOffset % SECTORS_PER_BLOCK != 0)
				file->fs->disk->read_sector(file->fs->disk, sectorOffset, sector);
			else
				sectorOffset = get_datablock(file, &inode, offset, sector)*SECTORS_PER_BLOCK + (offset / MAX_SECTOR_SIZE);
		}
		printf("%c", sector[offset % MAX_SECTOR_SIZE + i]);
	}
	return length;
}

/////////////////////////////////
int ext2_remove(EXT2_NODE* file) {
	return 0;
}

int ext2_create(EXT2_NODE* parent, const char* entryName, EXT2_NODE* retEntry) {
	BYTE         name[MAX_NAME_LENGTH] = { 0, };
	int          result;
	int            inodeNum;
	BYTE sector[MAX_SECTOR_SIZE];
	ENTRY_LOCATION* location = (ENTRY_LOCATION *)malloc(sizeof(ENTRY_LOCATION));
	EXT2_GROUP_DESC *entry;
	EXT2_INODE inode;
	UINT32 sectorOffset;
	UINT32 entryOffset;
	int i;

	strcpy(name, entryName);

	if (format_name(parent->fs, name))
		return EXT2_ERROR;

	/* newEntry */
	ZeroMemory(retEntry, sizeof(EXT2_NODE));
	memcpy(retEntry->entry.name, name, MAX_ENTRY_NAME_LENGTH);

	if (ext2_lookup(parent, name, location) == EXT2_SUCCESS)
		return EXT2_ERROR;

	retEntry->fs = parent->fs;

	inodeNum = alloc_free_inode(parent);
	retEntry->inodeNum = inodeNum;

	fill_entry(&retEntry->entry, name, retEntry->inodeNum);
	if (retEntry->entry.inode == EXT2_ERROR) {
		return EXT2_ERROR;
	}
	ext2_fill_inode(parent->fs, &inode, retEntry->entry.inode);

	name[0] = DIR_ENTRY_NO_MORE;
	for (i = 0; i < SECTORS_PER_BLOCK; i++) {
		if (ext2_lookup(parent, name, location) == EXT2_SUCCESS)
			break;
	}

	sectorOffset = location->entryNum * sizeof(EXT2_DIR_ENTRY) / MAX_SECTOR_SIZE;
	entryOffset = location->entryNum * sizeof(EXT2_DIR_ENTRY) % MAX_SECTOR_SIZE / sizeof(EXT2_DIR_ENTRY);

	parent->fs->disk->read_sector(parent->fs->disk, sectorOffset + location->blockNum*SECTORS_PER_BLOCK, sector);
	entry = (EXT2_DIR_ENTRY*)sector;
	entry += entryOffset;

	fill_entry(entry, retEntry->entry.name, retEntry->entry.inode);
	parent->fs->disk->write_sector(parent->fs->disk, sectorOffset + location->blockNum*SECTORS_PER_BLOCK, sector);

	retEntry->fs = parent->fs;

	return EXT2_SUCCESS;
}

/******************************************************************************/
/*                                                                            */
/* Project : EXT2 File System                                             */
/* File    : disk.h                                                           */
/* Author  : Kim yohan (kyh9369@q.ssu.ac.kr)                                   */
/* Company : soongsil Univ. network computing Lab.                            */
/* Notes   : ext2_shell                                          */
/* Date    : 2017/2/10                                                         */
/*                                                                            */
/******************************************************************************/


#include "ext2_shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

char* my_strncpy( char* dest, const char* src, int length )
{
	while( *src && *src != 0x20 && length-- > 0 )
		*dest++ = *src++;

	return dest;
}

int my_strnicmp( const char* str1, const char* str2, int length )
{
	char	c1, c2;

	while( ( ( *str1 && *str1 != 0x20 ) || ( *str2 && *str2 != 0x20 ) ) && length-- > 0 )
	{
		c1 = toupper( *str1 );
		c2 = toupper( *str2 );

		if( c1 > c2 )
			return -1;
		else if( c1 < c2 )
			return 1;

		str1++;
		str2++;
	}

	return 0;
}

int ext2_entry_to_shell_entry( const EXT2_NODE* ext2_entry, SHELL_ENTRY* shell_entry )
{
	EXT2_NODE* entry = ( EXT2_NODE* )shell_entry->pdata;
    EXT2_INODE inode;
	BYTE*	str;
	char buf[10];

	memset( shell_entry, 0, sizeof( SHELL_ENTRY ) );

    ext2_fill_inode(ext2_entry->fs, &inode, ext2_entry->entry.inode);
	if( inode.i_mode != ATTR_VOLUME_ID )
	{
		str = shell_entry->name;
		str = my_strncpy( str, ext2_entry->entry.name, 8 );
		if( ext2_entry->entry.name[8] != 0x20 )
		{
			str = my_strncpy( str, ".", 1 );
			str = my_strncpy( str, &ext2_entry->entry.name[8], 3 );
		}
	}

	if( inode.i_mode & ATTR_DIRECTORY ||
		inode.i_mode & ATTR_VOLUME_ID )
		shell_entry->isDirectory = 1;
	else
		shell_entry->size = inode.i_blocks * BLOCK_SIZE_IN_BYTES;
	
	*entry = *ext2_entry;
	memcpy(buf, entry->entry.name, 5);
	buf[6] = '\n';
	printf(buf);
	//memcpy(shell_entry->name, str, 1);
	return EXT2_SUCCESS;
}

int shell_entry_to_ext2_entry( const SHELL_ENTRY* shell_entry, EXT2_NODE* ext2_entry )
{
	EXT2_NODE* entry = ( EXT2_NODE* )shell_entry->pdata;

	*ext2_entry = *entry;

	return EXT2_SUCCESS;
}

int	fs_create( DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, const SHELL_ENTRY* parent, const char* name, SHELL_ENTRY* retEntry ){
	EXT2_NODE	EXT2Parent;
	EXT2_NODE	EXT2Entry;
	int			result;

	shell_entry_to_ext2_entry( parent, &EXT2Parent );

	result = ext2_create( &EXT2Parent, name, &EXT2Entry );

	if (result)
		return result;

	ext2_entry_to_shell_entry( &EXT2Entry, retEntry );

	return result;
}

int fs_remove( DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, const SHELL_ENTRY* parent, const char* name ){
	EXT2_NODE	EXT2Parent;
	EXT2_NODE	file;

	shell_entry_to_ext2_entry( parent, &EXT2Parent );
	ext2_lookup( &EXT2Parent, name, &file );

	return ext2_remove( &file );
}

int	fs_read( DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, const SHELL_ENTRY* parent, SHELL_ENTRY* entry, unsigned long offset, unsigned long length, char* buffer ){
	EXT2_NODE	EXT2Entry;

	shell_entry_to_ext2_entry( entry, &EXT2Entry );

	return ext2_read( &EXT2Entry, offset, length, buffer );
}

int	fs_write( DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, const SHELL_ENTRY* parent, SHELL_ENTRY* entry, unsigned long offset, unsigned long length, const char* buffer ){
	EXT2_NODE	EXT2Entry;

	shell_entry_to_ext2_entry( entry, &EXT2Entry );

	return ext2_write ( &EXT2Entry, offset, length, buffer );
}

static SHELL_FILE_OPERATIONS g_file =
{
	fs_create,
	fs_remove,
	fs_read,
	fs_write
};

int adder( void* list, EXT2_NODE* entry){
	SHELL_ENTRY_LIST* entrylist = (SHELL_ENTRY_LIST*) list;
	SHELL_ENTRY	  newEntry;

	ext2_entry_to_shell_entry( entry, &newEntry );

	add_entry_list( entrylist, &newEntry ); // list에 entry 추가
	
	return EXT2_SUCCESS;
}

int fs_read_dir( DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, const SHELL_ENTRY* parent, SHELL_ENTRY_LIST* list )
{
	EXT2_NODE entry;
	
	if( list->count )
		release_entry_list( list );

	shell_entry_to_ext2_entry( parent, &entry );
	ext2_read_dir( &entry, adder, list );
	
	return EXT2_SUCCESS;
}

int is_exist( DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, const SHELL_ENTRY* parent, const char *name){
	SHELL_ENTRY_LIST list;
	SHELL_ENTRY_LIST_ITEM* current;

	init_entry_list( &list );
	
	fs_read_dir( disk, fsOprs, parent, &list); //¿¿ ¿¿¿¿¿ ¿¿ ¿¿ list¿ ¿¿¿
	current = list.first;

	while(current){
		if( my_strnicmp( current->entry.name, name, 12) == 0 ){
			release_entry_list( &list );
			return EXT2_ERROR;
		}
		
		current = current->next;
	}
	
	release_entry_list( &list );
	return EXT2_SUCCESS;
}

int fs_stat( DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, unsigned int* totalSectors, unsigned int* usedSectors ){
	EXT2_NODE	entry;

	return ext2_df( fsOprs->pdata, totalSectors, usedSectors );//disk free
}

int fs_mkdir( DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, const SHELL_ENTRY* parent, const char* name, SHELL_ENTRY* retEntry ){
	EXT2_NODE	EXT2Parent;
	EXT2_NODE	EXT2Entry;
	int			result;

	if (is_exist( disk, fsOprs, parent, name ) )
		return EXT2_ERROR;

	shell_entry_to_ext2_entry( parent, &EXT2Parent );

	result = ext2_mkdir( &EXT2Parent, name, &EXT2Entry );

	if (result)
		return result;

	ext2_entry_to_shell_entry( &EXT2Entry, retEntry );
	
	return result;
}

int fs_rmdir( DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, const SHELL_ENTRY* parent, const char* name ){
	EXT2_NODE	EXT2Parent;
	EXT2_NODE	dir;

	shell_entry_to_ext2_entry( parent, &EXT2Parent );
	ext2_lookup( &EXT2Parent, name, &dir );

	return ext2_rmdir( &dir );
}

int fs_lookup( DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, const SHELL_ENTRY* parent, SHELL_ENTRY* entry, const char* name ){
	EXT2_NODE EXT2Parent;
	EXT2_NODE EXT2Entry;
	int			result;

	shell_entry_to_ext2_entry( parent, &EXT2Parent );

	result = ext2_lookup( &EXT2Parent , name, &EXT2Entry );

	ext2_entry_to_shell_entry(&EXT2Parent, parent);

	return result;
}

int fs_dump(DISK_OPERATIONS* disk, int blockGroupNum, int type, int target){
	return ext2_dump(disk,blockGroupNum, type, target);
}

int fs_dumpFile(DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, const SHELL_ENTRY* parent, const char* name){
	EXT2_NODE EXT2Parent;
	EXT2_NODE EXT2Entry;
	EXT2_INODE target;
	int i;
	if (!is_exist( disk, fsOprs, parent, name ) )
		return EXT2_ERROR;

	shell_entry_to_ext2_entry( parent, &EXT2Parent );
	ext2_lookup( &EXT2Parent, name, &EXT2Entry );
	ext2_fill_inode(EXT2Parent.fs, &target, EXT2Parent.entry.inode);
	printf("inode number : %d\n",EXT2Entry.inodeNum);
	for(i=0;i<target.i_blocks;i++)
		printf("iblock[i]\t:  %d",target.i_block[i]);
	for(;i<14;i++)
		printf("iblock[i]\t:  0");

	return EXT2_SUCCESS;
}

static SHELL_FS_OPERATIONS	g_fsOprs =
{
	fs_read_dir,
	fs_stat,
	fs_mkdir,
	fs_rmdir,
	fs_lookup,
	fs_dump,
	fs_dumpFile,
	&g_file,
	NULL
};

int fs_mount( DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs, SHELL_ENTRY* root )
{
	EXT2_FILESYSTEM* ext2;
	EXT2_NODE	ext2_entry;
	int		result;

	*fsOprs = g_fsOprs;

	fsOprs->pdata = malloc( sizeof( EXT2_FILESYSTEM ) );
	ext2 = (EXT2_FILESYSTEM*) fsOprs->pdata;
	ZeroMemory( ext2, sizeof( EXT2_FILESYSTEM ) );
	ext2->disk = disk;

	result = ext2_read_superblock( ext2, &ext2_entry );

	if( result == EXT2_SUCCESS )
	{
		printf( "type               : EXT2\n" );
//		printf( "root entry count       : %d\n", fat->bpb.rootEntryCount );
//		printf( "total sectors          : %u\n",  );
//		printf( "\n" );
	}

	ext2_entry_to_shell_entry( &ext2_entry, root );

	return result;
}

void fs_umount( DISK_OPERATIONS* disk, SHELL_FS_OPERATIONS* fsOprs )
{
	if( fsOprs && fsOprs->pdata )
	{
		//만약 pdata에 해당하는 구조체에 정리해야할 것이 있으면 추가함수 넣어줄것!
		free( fsOprs->pdata );
		fsOprs->pdata = 0;
	}
}

int fs_format( DISK_OPERATIONS* disk, void* param )
{
    //ext2 포멧밖에 안하므로, param을 쓰지않음.
	return ext2_format( disk );
}

static SHELL_FILESYSTEM g_ext2 =
{
    "EXT",
    fs_mount,
    fs_umount,
    fs_format
};

void shell_register_filesystem( SHELL_FILESYSTEM * FS)
{
    *FS = g_ext2;
}


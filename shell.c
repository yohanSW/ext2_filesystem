/******************************************************************************/
/*                                                                            */
/* Project : EXT2 File System                                             */
/* File    : disk.h                                                           */
/* Author  : Kim yohan (kyh9369@q.ssu.ac.kr)                                   */
/* Company : soongsil Univ. network computing Lab.                            */
/* Notes   : shell                                          */
/* Date    : 2017/2/10                                                         */
/*                                                                            */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "shell.h"
#include "disksim.h"

#define SECTOR_SIZE				512
#define NUMBER_OF_SECTORS		524288

#define COND_MOUNT				0x01
#define COND_UMOUNT				0x02

typedef struct
{
	char*	name;
	int		( *handler )( int, char** );
	char	conditions;
} COMMAND;

extern void shell_register_filesystem( SHELL_FILESYSTEM* );

void do_shell( void );
void unknown_command( void );
int seperate_string( char* buf, char* ptrs[] );

int shell_cmd_cd( int argc, char* argv[] );
int shell_cmd_exit( int argc, char* argv[] );
int shell_cmd_mount( int argc, char* argv[] );
int shell_cmd_umount( int argc, char* argv[] );
int shell_cmd_touch( int argc, char* argv[] );
int shell_cmd_fill( int argc, char* argv[] );
int shell_cmd_rm( int argc, char* argv[] );
int shell_cmd_ls( int argc, char* argv[] );
int shell_cmd_format( int argc, char* argv[] );
int shell_cmd_df( int argc, char* argv[] );
int shell_cmd_mkdir( int argc, char* argv[] );
int shell_cmd_rmdir( int argc, char* argv[] );
int shell_cmd_mkdirst( int argc, char* argv[] );
int shell_cmd_cat( int argc, char* argv[] );
int shell_cmd_dumpsb( int argc, char* argv[]);
int shell_cmd_dumpgdt( int argc, char* argv[]);
int shell_cmd_dumpbmap( int argc, char* argv[]);
int shell_cmd_dumpimap( int argc, char* argv[]);
int shell_cmd_dumpit( int argc, char* argv[]);
int shell_cmd_dumpdata( int argc, char* argv[]);
int shell_cmd_dumpfile( int argc, char* argv[]);

static COMMAND g_commands[] =
{
	{ "cd",		shell_cmd_cd,		COND_MOUNT	},
	{ "exit",	shell_cmd_exit,		0			},
	{ "quit",	shell_cmd_exit,		0			},
	{ "mount",	shell_cmd_mount,	COND_UMOUNT	},
	{ "umount",	shell_cmd_umount,	COND_MOUNT	},
	{ "touch",	shell_cmd_touch,	COND_MOUNT	},
	{ "fill",	shell_cmd_fill,		COND_MOUNT	},
	{ "rm",		shell_cmd_rm,		COND_MOUNT	},
	{ "ls",		shell_cmd_ls,		COND_MOUNT	},
	{ "dir",	shell_cmd_ls,		COND_MOUNT	},
	{ "format",	shell_cmd_format,	COND_UMOUNT	},
	{ "df",		shell_cmd_df,		COND_MOUNT	},
	{ "mkdir",	shell_cmd_mkdir,	COND_MOUNT	},
	{ "rmdir",	shell_cmd_rmdir,	COND_MOUNT	},
	{ "mkdirst",shell_cmd_mkdirst,	COND_MOUNT	},
	{ "cat",	shell_cmd_cat,		COND_MOUNT	},
	{ "dumpsb",					shell_cmd_dumpsb,		COND_MOUNT	},
	{ "dumpgdt",				shell_cmd_dumpgdt,		COND_MOUNT	},
	{ "dumpbmap",				shell_cmd_dumpbmap,		COND_MOUNT	},
	{ "dumpimap",				shell_cmd_dumpimap,		COND_MOUNT	},
	{ "dumpit",					shell_cmd_dumpit,		COND_MOUNT	},
	{ "dumpdata",				shell_cmd_dumpdata,		COND_MOUNT	},
	{ "dumpfileinode",			shell_cmd_dumpfile,		COND_MOUNT	}
};

static SHELL_FILESYSTEM		g_fs;
static SHELL_FS_OPERATIONS	g_fsOprs;
static SHELL_ENTRY			g_rootDir;
static SHELL_ENTRY			g_currentDir;
static DISK_OPERATIONS		g_disk;

int g_commandsCount = sizeof( g_commands ) / sizeof( COMMAND );
int g_isMounted;

int main( int argc, char* argv[] )
{
	if( disksim_init( NUMBER_OF_SECTORS, SECTOR_SIZE, &g_disk ) < 0 )
	{
		printf( "disk simulator initialization has been failed\n" );
		return -1;
	}

	shell_register_filesystem( &g_fs );

	do_shell();

	return 0;
}

int check_conditions( int conditions )
{
	if( conditions & COND_MOUNT && !g_isMounted )
	{
		printf( "file system is not mounted\n" );
		return -1;
	}

	if( conditions & COND_UMOUNT && g_isMounted )
	{
		printf( "file system is already mounted\n" );
		return -1;
	}

	return 0;
}

void do_shell( void )
{
	char buf[1000];
	char command[100];
	char* argv[100];
	int argc;
	int i;

	printf( "%s File system shell\n", g_fs.name );

	while( -1 )
	{
		printf( "[%s/]# ", g_currentDir.name );
		fgets( buf, 1000, stdin );

		argc = seperate_string( buf, argv );

		if( argc == 0 )
			continue;

		for( i = 0; i < g_commandsCount; i++ )
		{
			if( strcmp( g_commands[i].name, argv[0] ) == 0 )
			{
				if( check_conditions( g_commands[i].conditions ) == 0 )
					g_commands[i].handler( argc, argv );

				break;
			}
		}
		if( argc != 0 && i == g_commandsCount )
			unknown_command();
	}
}

void unknown_command( void )
{
	int i;

	printf( " * " );
	for( i = 0; i < g_commandsCount; i++ )
	{
		if( i < g_commandsCount - 1 )
			printf( "%s, ", g_commands[i].name );
		else
			printf( "%s", g_commands[i].name );
	}
	printf( "\n" );
}

int seperate_string( char* buf, char* ptrs[] )
{
	char prev = 0;
	int count = 0;

	while( *buf )
	{
		if( isspace( *buf ) )
			*buf = 0;
		else if( prev == 0 )	/* continually space */
			ptrs[count++] = buf;

		prev = *buf++;
	}

	return count;
}

/******************************************************************************/
/* Shell commands...                                                          */
/******************************************************************************/

int shell_cmd_cd( int argc, char* argv[] )
{
	SHELL_ENTRY	newEntry;
	int			result;
	static SHELL_ENTRY	path[256];
	static int			pathTop = 0;

	path[0] = g_rootDir;

	if( argc > 2 )
	{
		printf( "usage : %s [directory]\n", argv[0] );
		return 0;
	}

	if( argc == 1 )
		pathTop = 0;
	else
	{
		if( strcmp( argv[1], "." ) == 0 )
			return 0;
		else if( strcmp( argv[1], ".." ) == 0 && pathTop > 0 )
			pathTop--;
		else
		{
			result = g_fsOprs.lookup( &g_disk, &g_fsOprs, &g_currentDir, &newEntry, argv[1] );

			if( result==EXT2_SUCCESS )
			{
				printf( "directory not found\n" );
				return -1;
			}
			else if( !newEntry.isDirectory )
			{
				printf( "%s is not a directory\n", argv[1] );
				return -1;
			}
			path[++pathTop] = newEntry;
			//printf("%d ", pathTop);

		}
	}

	g_currentDir = path[pathTop];

	return 0;
}

int shell_cmd_exit( int argc, char* argv[] )
{
	disksim_uninit( &g_disk );
	_exit( 0 );

	return 0;
}

int shell_cmd_mount( int argc, char* argv[] )
{
	int result;

	if( g_fs.mount == NULL )
	{
		printf( "The mount functions is NULL\n" );
		return 0;
	}

	result = g_fs.mount( &g_disk, &g_fsOprs, &g_rootDir );
	g_currentDir = g_rootDir;

	if( result < 0 )
	{
		printf( "%s file system mounting has been failed\n", g_fs.name );
		return -1;
	}
	else
	{
		printf( "%s file system has been mounted successfully\n", g_fs.name );
		g_isMounted = 1;
	}

	return 0;
}

int shell_cmd_umount( int argc, char* argv[] )
{
	g_isMounted = 0;

	if( g_fs.umount == NULL )
		return 0;

	g_fs.umount( &g_disk, &g_fsOprs );
	return 0;
}

int shell_cmd_touch( int argc, char* argv[] )
{
	SHELL_ENTRY	entry;
	int			result;

	if( argc < 2 )
	{
		printf( "usage : touch [files...]\n" );
		return 0;
	}

	result = g_fsOprs.fileOprs->create( &g_disk, &g_fsOprs, &g_currentDir, argv[1], &entry );

	if( result )
	{
		printf( "create failed\n" );
		return -1;
	}

	return 0;
}

int shell_cmd_fill( int argc, char* argv[] )
{
	SHELL_ENTRY	entry;
	char*		buffer;
	char*		tmp;
	int			size;
	int			result;

	if( argc != 3 )
	{
		printf( "usage : fill [file] [size]\n" );
		return 0;
	}

	scanf( argv[2], "%d", &size );

	result = g_fsOprs.fileOprs->create( &g_disk, &g_fsOprs, &g_currentDir, argv[1], &entry );
	if( result )
	{
		printf( "create failed\n" );
		return -1;
	}

	buffer = ( char* )malloc( size + 13 );
	tmp = buffer;
	while( tmp < buffer + size )
	{
		memcpy( tmp, "Can you see? ", 13 );
		tmp += 13;
	}
	g_fsOprs.fileOprs->write( &g_disk, &g_fsOprs, &g_currentDir, &entry, 0, size, buffer );
	free( buffer );

	return 0;
}

int shell_cmd_rm( int argc, char* argv[] )
{
	int i;

	if( argc < 2 )
	{
		printf( "usage : rm [files...]\n" );
		return 0;
	}

	for( i = 1; i < argc; i++ )
	{
		if( g_fsOprs.fileOprs->remove( &g_disk, &g_fsOprs, &g_currentDir, argv[i] ) )
			printf( "cannot remove file\n" );
	}

	return 0;
}

int shell_cmd_ls( int argc, char* argv[] )
{
	SHELL_ENTRY_LIST		list;
	SHELL_ENTRY_LIST_ITEM*	current;

	if( argc > 2 )
	{
		printf( "usage : %s [path]\n", argv[0] );
		return 0;
	}

	init_entry_list( &list );
	if( g_fsOprs.read_dir( &g_disk, &g_fsOprs, &g_currentDir, &list ) )
	{
		printf( "Failed to read_dir\n" );
		return -1;
	}

	current = list.first;

	printf( "[File names] [D] [File sizes]\n" );
	while( current )
	{
		printf( "%-12s  %1d  %12d\n",
				current->entry.name, current->entry.isDirectory, current->entry.size );
		current = current->next;
	}
	printf( "\n" );

	release_entry_list( &list );
	return 0;
}

int shell_cmd_format( int argc, char* argv[] )
{
	int		result;
	char*	param = NULL;

	if( argc >= 2 )
		param = argv[1];

	result = g_fs.format( &g_disk, param );

	if( result < 0 )
	{
		printf( "%s formatting is failed\n", g_fs.name );
		return -1;
	}

	printf( "disk has been formatted successfully\n" );
	return 0;
}

double get_percentage( unsigned int number, unsigned int total )
{
	return ( ( double )number ) / total * 100.;
}

int shell_cmd_df( int argc, char* argv[] )
{
	unsigned int used, total;
	int result;

	g_fsOprs.stat( &g_disk, &g_fsOprs, &total, &used );

	printf( "free sectors : %u(%.2lf%%)\tused sectors : %u(%.2lf%%)\ttotal : %u\n",
			total - used, get_percentage( total - used, g_disk.numberOfSectors ),
		   	used, get_percentage( used, g_disk.numberOfSectors ),
		   	total );

	return 0;
}

int shell_cmd_mkdir( int argc, char* argv[] )
{
	SHELL_ENTRY	entry;
	int result;

	if( argc != 2 )
	{
		printf( "usage : %s [name]\n", argv[0] );
		return 0;
	}
	
	result = g_fsOprs.mkdir( &g_disk, &g_fsOprs, &g_currentDir, argv[1], &entry );

	if( result )
	{
		printf( "cannot create directory\n" );
		return -1;
	}

	return 0;
}

int shell_cmd_rmdir( int argc, char* argv[] )
{
	int result;

	if( argc != 2 )
	{
		printf( "usage : %s [name]\n", argv[0] );
		return 0;
	}

	result = g_fsOprs.rmdir( &g_disk, &g_fsOprs, &g_currentDir, argv[1] );

	if( result )
	{
		printf( "cannot remove directory\n" );
		return -1;
	}

	return 0;
}

int shell_cmd_mkdirst( int argc, char* argv[] )
{
	SHELL_ENTRY	entry;
	int		result, i, count;
	char	buf[10];

	if( argc != 2 )
	{
		printf( "usage : %s [count]\n", argv[0] );
		return 0;
	}

	scanf( argv[1], "%d", &count );
	for( i = 0; i < count; i++ )
	{
		printf( buf, "%d", i );
		result = g_fsOprs.mkdir( &g_disk, &g_fsOprs, &g_currentDir, buf, &entry );

		if( result )
		{
			printf( "cannot create directory\n" );
			return -1;
		}
	}

	return 0;
}

int shell_cmd_cat( int argc, char* argv[] )
{
	SHELL_ENTRY	entry;
	char		buf[1025] = { 0, };
	int			result;
	unsigned long	offset = 0;

	if( argc != 2 )
	{
		printf( "usage : %s [file name]\n", argv[0] );
		return 0;
	}

	result = g_fsOprs.lookup( &g_disk, &g_fsOprs, &g_currentDir, &entry, argv[1] );
	if( result )
	{
		printf( "%s lookup failed\n", argv[1] );
		return -1;
	}

	while( g_fsOprs.fileOprs->read( &g_disk, &g_fsOprs, &g_currentDir, &entry, offset, 1024, buf ) > 0 )
	{
		printf( "%s", buf );
		offset += 1024;
		memset( buf, 0, sizeof( buf ) );
	}
	printf( "\n" );
}

/******************************************************************************/
/* Shell dump commands...                                                          */
/******************************************************************************/

int shell_cmd_dumpsb( int argc, char* argv[])
{
	int result;

    result = g_fsOprs.lookdump( &g_disk, 0 , 1 , 0);

	if( result )
	{
		printf( "cannot look superBlock's dump\n" );
		return -1;
	}

	return 0;
/*int i;
	SECTOR* superblockSector[SECTOR_SIZE * 2];
	g_disk->read_sector(g_disk,0,superblockSector);
	g_disk->read_sector(g_disk,1,&superblockSector[SECTOR_SIZE]);

	printf("start adress : %p , end address : %p\n\n\n",superblockSector,superblockSector+SECTOR_SIZE*2);
	
	for(i=0;i<SECTOR_SIZE * 2;i++){
		if(i%16 == 0)
			printf("%p\t%x ",&superblockSector[i],superblockSector[i]);
		else if(i % 16==15)
			printf(" %x \n",superblockSector[i]);
		else
			printf(" %x ",superblockSector[i]);
	}
	retrun 0;*/
}

int shell_cmd_dumpgdt( int argc, char* argv[])
{
	int result;

	result = g_fsOprs.lookdump( &g_disk, 0 , 2 , 0);

	if( result )
	{
		printf( "cannot look group description block's dump\n" );
		return -1;
	}

	return 0;
}

int shell_cmd_dumpbmap( int argc, char* argv[])
{
	int result;

	if( argc != 2 )
	{
		printf( "usage : %s [block group number]\n", argv[0] );
		return 0;
	}
    result = atoi(argv[1]);
	result = g_fsOprs.lookdump( &g_disk, result , 3 , 0);

	if( result )
	{
		printf( "cannot look block bitmap's dump\n" );
		return -1;
	}

	return 0;
}

int shell_cmd_dumpimap( int argc, char* argv[])
{
	int result;

	if( argc != 2 )
	{
		printf( "usage : %s [block group number]\n", argv[0] );
		return 0;
	}
    result = atoi(argv[1]);
	result = g_fsOprs.lookdump( &g_disk, result , 4 , 0);

	if( result )
	{
		printf( "cannot look inode bitmap's dump\n" );
		return -1;
	}

	return 0;
}

int shell_cmd_dumpit( int argc, char* argv[])
{
	int result;

	if( argc != 2 )
	{
		printf( "usage : %s [block group number]\n", argv[0] );
		return 0;
	}
    result = atoi(argv[1]);
	result = g_fsOprs.lookdump( &g_disk, result , 5 , 0);

	if( result )
	{
		printf( "cannot look inode table's dump\n" );
		return -1;
	}

	return 0;
}

int shell_cmd_dumpdata(int argc, char* argv[])
{
	int result, blockNum;

	if (argc != 3)
	{
		printf("usage : %s [block group num] [data block number]\n", argv[0]);
		return 0;
	}
	result = atoi(argv[1]);
	blockNum = atoi(argv[2]);
	result = g_fsOprs.lookdump(&g_disk, result, 6, blockNum);

	if (result)
	{
		printf("cannot look data block's dump\n");
		return -1;
	}

	return 0;
}

int shell_cmd_dumpfile( int argc, char* argv[])
{
	int result, fileNum;

	if( argc != 2 )
	{
		printf( "usage : %s [filename]\n", argv[0] );
		return 0;
	}

	result = g_fsOprs.lookfile( &g_disk, argv[1] );

	if( result )
	{
		printf( "cannot look file's dump\n" );
		return -1;
	}

	return 0;
}


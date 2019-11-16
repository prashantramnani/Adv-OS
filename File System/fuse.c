#define FUSE_USE_VERSION 30
#include <fuse.h>
#include "sfs.h"
#include "disk.h"
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

// http://www.maastaar.net/fuse/linux/filesystem/c/2016/05/21/writing-a-simple-filesystem-using-fuse/
static int do_getattr( const char *path, struct stat *st )
{
    st->st_uid = getuid(); // The owner of the file/directory is the user who mounted the filesystem
	st->st_gid = getgid(); // The group of the file/directory is the same as the group of the user who mounted the filesystem
	st->st_atime = time( NULL ); // The last "a"ccess of the file/directory is right now
	st->st_mtime = time( NULL ); // The last "m"odification of the file/directory is right now

	if ( strcmp( path, "/" ) == 0 || is_file((char *) path ) == 1 )
	{
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2; // Why "two" hardlinks instead of "one"? The answer is here: http://unix.stackexchange.com/a/101536
	}
	else if ( is_file((char *) path )  == 0 )
	{
		st->st_mode = S_IFREG | 0755;
		st->st_nlink = 1;
		st->st_size = 0;
	}
	else
	{
		return -ENOENT;
	}
	
	return 0;
}

static int do_read( const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi )
{
	int x = read_file((char *)path, buffer, size, offset);
	printf("Read content = %s\n", buffer);
	return x;
}

static int do_mkdir( const char *path, mode_t mode )
{
	if (create_dir((char *)path) < 0) { 
        return -1;
    }
	return 0;
}

static int do_rmdir( const char *path)
{
	return remove_dir((char *)path);
}

static int do_write( const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *info )
{
	return write_file((char *)path, (char *)buffer, size, offset);
}

static int do_mknod( const char *path, mode_t mode, dev_t rdev )
{
	if (open_file((char *)path) < 0)
		return -1;
	return 0;
}

static struct fuse_operations operations = {
   .getattr	= do_getattr,
	.read		= do_read,
    .write		= do_write,
    .mkdir		= do_mkdir,
    .mknod      = do_mknod,
    .rmdir 		= do_rmdir,
};

int main( int argc, char *argv[] )
{
	char *fname = "test_disk_file1", *fname2 = "test_disk_file2";
    uint32_t nbytes = 409616;
    assert(create_disk(fname, nbytes) == 0);
    int fd1 = open_disk(fname);

    assert(format(fd1) >= 0);
    assert(mount_sfs(fd1) >= 0);

	return fuse_main( argc, argv, &operations, NULL );
}

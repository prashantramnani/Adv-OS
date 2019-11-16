#include "sfs.h"
#include "disk.h"
#include <assert.h>

int main(){
    char *fname = "test_disk_file1", *fname2 = "test_disk_file2";
    uint32_t nbytes = 409616;
    assert(create_disk(fname, nbytes) == 0);
    int fd1 = open_disk(fname);
	
	disk_stat* disk_info = get_disk_stat(fd1);
	printf("Disk size:%u\n", disk_info->size);
	
	if (format(fd1) < 0)
        printf("Unable to format\n");
    assert(mount_sfs(fd1) >= 0);

	super_block *sb = (super_block *)malloc(BLOCKSIZE);
    if (read_block(fd1, 0, sb) < 0)
        printf("reading sb failed\n");

    printf("inode of apple : %d\n", create_dir("apple"));
    printf("inode of apple/ball : %d\n", create_dir("apple/ball"));
    printf("inode of apple/cat : %d\n", create_dir("apple/cat"));

    char data[10];
    int i;
    for (i = 0; i < 10; i++)
    	data[i] = (char)('0' + i);

    assert(write_file("apple/ball/dog", data, 10, 0) == 10);
    assert(write_file("apple/cat/elephant", data, 10, 0) == 10);

    char rdata[10];
    assert(read_file("apple/ball/dog", rdata, 10, 0) == 10);
	assert(strncmp(data, rdata, 10) == 0);
    
	assert(read_file("apple/cat/elephant", rdata, 10, 0) == 10);
	assert(strncmp(data, rdata, 10) == 0);
     
	assert(remove_dir("apple/cat") == 0);
	assert(read_file("apple/cat/elephant", rdata, 10, 0) < 0);

    close_disk(fd1);

	free(sb);

	printf("All tests successful\n");
	return 0;
}

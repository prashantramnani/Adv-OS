#include "sfs.h"
#include "disk.h"
#include <assert.h>

int main()
{
    char *fname = "test_disk_file1", *fname2 = "test_disk_file2";
    uint32_t nbytes = 409616;

    assert(create_disk(fname, nbytes) == 0);
    int fd1 = open_disk(fname);

    assert(format(fd1) >= 0);
    assert(mount_sfs(fd1) >= 0);

    int inum = create_file();
    assert(inum == 1);
    assert(stats(inum) >= 0);

    int off = 0, i;
    char data[10];
    for (i = 0; i < 10; i++)
    	data[i] = (char)('0' + i);

    int ret = write_i(inum, data, 10, off);
    assert(ret == 10);

    char data2[10];
    ret = read_i(inum, data2, 10, off);
    assert(ret == 10);

    for (i = 0; i < 10; i++)
    	assert(data2[i] == data[i]);

    assert(stats(inum) >= 0);
    assert(remove_file(inum) >= 0);
    close_disk(fd1);

    printf("Tests successful\n");

}

#include "disk.h"
#include <assert.h>

int main()
{
    char *fname = "test_disk_file1", *fname2 = "test_disk_file2";
    uint32_t nbytes = 409616;
    assert(create_disk(fname, nbytes) == 0);
    int fd1 = open_disk(fname);

    assert(create_disk(fname2, nbytes - 16) >= 0);
    int fd2 = open_disk(fname2);

    assert(fd1 > 0 && fd2 > 0);
    disk_stat* p = get_disk_stat(fd1);
    assert(p->size == nbytes);
    assert(p->reads == 0 && p->writes == 0 && p->blocks == 100);

    disk_stat* p2 = get_disk_stat(fd2);
    assert(p2->size == (nbytes - 16));
    assert(p2->reads == 0 && p2->writes == 0 && p2->blocks == 99);
     
    char buff[BLOCKSIZE];
    assert(read_block(fd1, 1, buff) == 0);

    p = get_disk_stat(fd1);
    assert(p->size == nbytes);
    assert(p->reads == 1 && p->writes == 0 && p->blocks == 100);

    char buff2[BLOCKSIZE];
    int i;
    for (i = 0; i < BLOCKSIZE - 1; i++)
        buff2[i] = (char)((i%26) + 'a');
    buff2[BLOCKSIZE - 1] = '\0';

    assert(write_block(fd1, 1, buff2) == 0);

    p = get_disk_stat(fd1);
    assert(p->size == nbytes);
    assert(p->reads == 1 && p->writes == 1 && p->blocks == 100);
    
    assert(read_block(fd1, 1, buff) == 0);
    
    p = get_disk_stat(fd1);
    assert(p->size == nbytes);
    assert(p->reads == 2 && p->writes == 1 && p->blocks == 100);
   
    assert(strcmp(buff, buff2) == 0);
    assert(close_disk(fd1) == 0);
    assert(close_disk(fd2) == 0);

    printf("Test disk successfull");
}


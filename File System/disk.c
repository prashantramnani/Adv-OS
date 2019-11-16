#include "disk.h"
#include <sys/stat.h>
#include <fcntl.h>

static disk_list *head = NULL;

disk_list* search_file(char *filename)
{
    if (head == NULL)
        return NULL;
    
    disk_list *p;
    for (p = head; p != NULL; p = p->next) 
        if (strcmp(p->filename, filename) == 0)
            return p;

    return NULL;
}

void add_to_list(char *filename, int fd)
{
    disk_list *p;
    if (head == NULL) {
        head = (disk_list *) malloc(sizeof(disk_list));
        p = head;
    } else {
        for (p = head; p->next != NULL; p = p->next)
            p = p->next;
        p->next = (disk_list *) malloc(sizeof(disk_list));
        p = p->next;
    }
    p->filename = (char *) malloc(sizeof(char) * strlen(filename));
    strcpy(p->filename, filename);
    
    p->fd = fd;
}

int del_from_list(int fd)
{
    if (head == NULL)
        return -ERR;

    disk_list *p, *q;
    for (q = NULL, p = head; p != NULL; q = p, p = p->next)
        if (p->fd == fd) {
            if (q == NULL)
                head = p->next;
            else
                q->next = p->next;
            close(fd);
            free(p);

            return 0;
        }

    return -ERR; // Not found diskno.
}

int create_disk(char *filename, int nbytes)
{
    disk_list* p = search_file(filename);
    if (p) {
        del_from_list(p->fd);
        // return -ERR;
    }
    
    int fd = open(filename, O_RDWR|O_CREAT|O_TRUNC, 0777);
    if (fd < 0)
        return -ERR;

    add_to_list(filename, fd);
    ftruncate(fd, nbytes);

    disk_stat stats;
    stats.size = nbytes;
    stats.blocks = (nbytes - sizeof(disk_stat))/ BLOCKSIZE; 
    stats.reads = stats.writes = 0;
   
    write(fd, &stats, sizeof(disk_stat));

    printf("Created disk of size - %u, with blocks = %u\n", stats.size, stats.blocks);
    return 0; 
}

int open_disk(char* filename)
{
    disk_list* p = search_file(filename);
    if (p == NULL)
        return -ERR;

    return p->fd;
}

disk_stat* get_disk_stat(int disk)
{
    if (disk <= 0)
        return NULL;

    disk_stat *p = (disk_stat*) malloc(sizeof(disk_stat));    
    lseek(disk, 0, SEEK_SET);
    if (read(disk, p, sizeof(disk_stat)) < 0)
        return NULL;

    return p;
}

int update_disk_stat(disk_stat *p, int fd)
{
    lseek(fd, 0, SEEK_SET);
    write(fd, p, sizeof(disk_stat));

    free(p);
    return 0;
}

int read_block(int disk, int blocknr, void *block_data)
{
    disk_stat* p = get_disk_stat(disk);
    if (p == NULL || blocknr < 0 || blocknr >= p->blocks)
        return -ERR;

    lseek(disk, sizeof(disk_stat) + blocknr * BLOCKSIZE, SEEK_SET);

    if (read(disk, block_data, BLOCKSIZE) < 0)
        return -ERR;
    p->reads++;
    return update_disk_stat(p, disk);
}

int write_block(int disk, int blocknr, void *block_data)
{
    disk_stat *p = get_disk_stat(disk);
    if (p == NULL || blocknr < 0 || blocknr >= p->blocks)
        return -ERR;

    lseek(disk, sizeof(disk_stat) + blocknr * BLOCKSIZE, SEEK_SET);
    
    if (write(disk, block_data, BLOCKSIZE) < 0)
        return -ERR;
    p->writes++;
    return update_disk_stat(p, disk);
}

int close_disk(int disk)
{
    return del_from_list(disk);
}


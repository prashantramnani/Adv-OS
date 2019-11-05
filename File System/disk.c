#include "disk.h"

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

static int _fread(void *ptr, size_t size, FILE *fp)
{
    fread(ptr, size, 1, fp);
    if (ferror(fp))
        return -ERR;
    
    return 0;
}

static int _fwrite(void *ptr, size_t size, FILE *fp)
{
    fwrite(ptr, size, 1, fp);
    if (ferror(fp))
        return -ERR;

    fseek(fp, 0, SEEK_SET);
    return 0;
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

int create_disk(char *filename, int nbytes)
{
    if (search_file(filename) != NULL)
        return -ERR;
    
    FILE *fp = fopen(filename, "wb+");
    if (fp == NULL)
        return -ERR;

    int fd = fileno(fp);
    add_to_list(filename, fd);
    ftruncate(fd, nbytes);

    disk_stat stats;
    stats.size = nbytes;
    stats.blocks = (nbytes - sizeof(disk_stat))/ BLOCKSIZE; 
    stats.reads = stats.writes = 0;
   
    return _fwrite(&stats, sizeof(disk_stat), fp);
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
    FILE* fp = fdopen(disk, "r");
    if (fp == NULL)
        return NULL;

    fseek(fp, 0, SEEK_SET);

    disk_stat *p = (disk_stat*) malloc(sizeof(disk_stat));
    
    if (_fread(p, sizeof(disk_stat), fp) < 0)
        return NULL;

    return p;
}

int update_disk_stat(disk_stat *p, FILE *fp)
{
    if (p == NULL) 
        return -ERR;
    fseek(fp, 0, SEEK_SET);
    return _fwrite(p, sizeof(disk_stat), fp);
}

int read_block(int disk, int blocknr, void *block_data)
{
    if (disk == 0)
        return -ERR;
    disk_stat* p = get_disk_stat(disk);
    if (p == NULL)
        return -ERR;

    if (blocknr < 0 || blocknr >= p->blocks)
        return -ERR;

    FILE *fp = fdopen(disk, "wb+");
    fseek(fp, blocknr * BLOCKSIZE, DISK_SEEK_SET);

    if (_fread(block_data, BLOCKSIZE, fp) < 0)
        return -ERR;
   
    p->reads++;
    return update_disk_stat(p, fp);
}

int write_block(int disk, int blocknr, void *block_data)
{
    if (disk == 0)
        return -ERR;
    disk_stat *p = get_disk_stat(disk);
    if (p == NULL)
        return -ERR;

    if (blocknr < 0 || blocknr >= p->blocks)
        return -ERR;

    FILE *fp = fdopen(disk, "wb+");
    fseek(fp, blocknr * BLOCKSIZE, DISK_SEEK_SET);
    
    if (_fwrite(block_data, BLOCKSIZE, fp) < 0)
        return -ERR;

    p->writes++;
    return update_disk_stat(p, fp);
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
            fclose(fdopen(p->fd, "r"));
            free(p);

            return 0;
        }

    return -ERR; // Not found diskno.
}

int close_disk(int disk)
{
    return del_from_list(disk);
}


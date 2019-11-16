#include "sfs.h"
#include "disk.h"

#define SetBit(A,k)     ( A |= (1 << k) )
#define ClearBit(A,k)   ( A &= ~(1 << k) )
#define TestBit(A,k)    ( A & (1 << k) )
#define max(x, y) (((x) > (y)) ? (x) : (y))
#define min(x, y) (((x) < (y)) ? (x) : (y))

static const int max_dir_entries = 1000;

uint32_t _ceil(uint32_t a, uint32_t b) {
	if (a % b == 0) return a / b;
	return a / b + 1;
}

void printD_superblock(super_block *sb)
{
    printf("Contents of super_block\n");
    printf("Magic number = %u\n", sb->magic_number);
    printf("blocks = %u\n", sb->blocks);
    printf("inode_blocks = %u\n", sb->inode_blocks);
    printf("inodes = %u\n", sb->inodes);
    printf("inode_bitmap_block_idx = %u\n", sb->inode_bitmap_block_idx);
    printf("data_block_bitmap_idx = %u\n", sb->data_block_bitmap_idx);
    printf("data_block_idx = %u\n", sb->data_block_idx);
    printf("data_blocks = %u\n\n", sb->data_blocks);
}

super_block init_super_block(int disk_blocks)
{
    super_block sb;
    sb.magic_number = MAGIC;
    sb.blocks = disk_blocks - 1; // 1 for super block

    sb.inode_blocks = INODE_FRAC * sb.blocks;
    sb.inodes = sb.inode_blocks * INODES_PER_BLOCK; 
    sb.inode_bitmap_block_idx = 1;

    uint32_t I = sb.inode_blocks;
    uint32_t IB = _ceil(I, 8 * BLOCKSIZE);
    uint32_t R = sb.blocks - I - IB;
    uint32_t DBB = _ceil(R, 8 * BLOCKSIZE); // only DB bits are valid
    uint32_t DB = R - DBB;
    //printf("%u %u %u %u %u\n", I, IB, R, DBB, DB);
    sb.inode_block_idx = 1 + IB + DBB;
    sb.data_block_bitmap_idx = 1 + IB;
    sb.data_block_idx = 1 + IB + DBB + I;
    sb.data_blocks = DB;

    return sb;
}

block_ty get_ty(int idx, const super_block *sb)
{
    if (idx < 0 || idx > sb->blocks)
        return UNDEF;

    if (idx == 0) return SUPERBLOCK;
    if (idx >= 1 && idx < sb->data_block_bitmap_idx) return INODE_BM;
    if (idx >= sb->data_block_bitmap_idx && idx < sb->inode_block_idx) return DATABLOCK_BM;
    if (idx >= sb->inode_block_idx && idx < sb->data_block_idx) return INODE;
    if (idx >= sb->data_block_idx && idx < (sb->data_block_idx + sb->data_blocks)) return DATABLOCK;
    return UNDEF;
}

void init_bitmap(int bmap[], int n)
{
    memset(bmap, 0, sizeof(uint32_t) * n);
}

void init_inode(inode in[], int n)
{
    for (int i = 0; i < n; i++)
        in[i].valid = 0;
}

void init_fs(super_block *sb, int disk)
{
    fs.sb = *sb;
    fs.len_inodes_bmap = sb->inodes / 32; // no of ints to cover all inodes
    fs.len_db_bmap = sb->data_blocks / 32; // no of ints to cover all dbs
    //printf("lens of bmaps : %u %u\n", fs.len_inodes_bmap, fs.len_db_bmap);

    fs.inode_bmap = (int *)malloc(sizeof(int) * fs.len_inodes_bmap);
    fs.db_bmap = (int *)malloc(sizeof(int) * fs.len_db_bmap);
    fs.disk = disk;
}

int format(int disk)
{
    disk_stat* stat = get_disk_stat(disk);
    if (stat == NULL)
        return -ERR;

    super_block sb = init_super_block(stat->blocks);
    printD_superblock(&sb);
    if (write_block(disk, 0, &sb) < 0)
        return -ERR;
    
    // init_fs(&sb, disk);

    uint32_t init_blocks = sb.data_block_idx - 1; // #blocks to be initialised

    for (int i = 1; i <= init_blocks; i++) {
        Block block;
        block_ty ty = get_ty(i, &sb);
       
        if (ty == INODE_BM || ty == DATABLOCK_BM)
            init_bitmap(block.bmap, PTRS_PER_BLOCK);
        else if (ty == INODE)
            init_inode(block.in, INODES_PER_BLOCK);
        else {
            printf("unexpected block type = %d\n", ty);
            return -ERR;
        }

        if (write_block(disk, i, &block) < 0)
            return -ERR;
    }
    
    return 0;
}

int mount_sfs(int disk)
{
    super_block sb;
    if (read_block(disk, 0, &sb) < 0)
        return -ERR;    

    if (sb.magic_number != MAGIC) 
        return -ERR;

    init_fs(&sb, disk);

    for (uint32_t i = 1; i < sb.inode_block_idx; i++) {
        Block block;
        read_block(disk, i, &block);

        block_ty ty = get_ty(i, &sb);
        void *to;
        
        if (ty == INODE_BM) {
            // printf("inode index: %u\n", PTRS_PER_BLOCK * (i - sb.inode_bitmap_block_idx));
            to = &fs.inode_bmap[PTRS_PER_BLOCK * (i - sb.inode_bitmap_block_idx)];
        }
        else if (ty == DATABLOCK_BM) {
            // printf("db index: %u\n", PTRS_PER_BLOCK * (i - sb.data_block_bitmap_idx));
            to = &fs.db_bmap[PTRS_PER_BLOCK * (i - sb.data_block_bitmap_idx)];
        }
        else {
            printf("unexpected block type = %d\n", ty);
            return -ERR;
        }

        memcpy(block.bmap, to, BLOCKSIZE);
    }
    create_file(); // root_inode

    return 0;
}

int clear_entry(uint32_t *bmap, uint32_t len, uint32_t idx, uint32_t bitmap_block_idx)
{
    if (idx < 0 || idx >= len)
        return -ERR;
    if (TestBit(bmap[idx/32], idx % 32) == 0)
        return -ERR;
    ClearBit(bmap[idx/32], idx % 32);

    uint32_t block_num = (idx / 32) / PTRS_PER_BLOCK, offset = (idx / 32) % PTRS_PER_BLOCK, bit_pos = idx % 32;

    uint32_t d_bmap[PTRS_PER_BLOCK]; // disk_bmap
    if (read_block(fs.disk, block_num + bitmap_block_idx, d_bmap) < 0)
        return -ERR;

    ClearBit(d_bmap[offset], bit_pos);
    write_block(fs.disk, block_num + bitmap_block_idx, d_bmap);

    return 0;
}

int clear_data_block_entry(uint32_t idx)
{
   return clear_entry(fs.db_bmap, fs.len_db_bmap, idx, fs.sb.data_block_bitmap_idx);
}

int clear_inode_bitmap_entry(uint32_t idx)
{
   return clear_entry(fs.inode_bmap, fs.len_inodes_bmap, idx, fs.sb.inode_bitmap_block_idx);
}

uint32_t get_free_entry(uint32_t *bmap, uint32_t len, uint32_t bitmap_block_idx, int *ret)
{
    uint32_t i, k, entry;
    *ret = -ERR;
    for (i = 0; i < len; i++)
        for (k = 0; k < 32; k++)
            if (!TestBit(bmap[i], k)) {
                //printf("%d %d %d\n", i, k, bmap[i]);
                SetBit(bmap[i], k);
                *ret = 0;
               // printf("%d %d %d\n", i, k, bmap[i]);
                entry = i * 32 + k;
                
                i = len;
                break;
            }

    if (*ret == -ERR) {
        printf("Out of space! No free entry left\n");
        return 0;
    }
 
    uint32_t block_num = (entry / 32) / PTRS_PER_BLOCK, offset = (entry / 32) % PTRS_PER_BLOCK, bit_pos = entry % 32;

    uint32_t d_bmap[PTRS_PER_BLOCK]; // disk_bmap
    if (read_block(fs.disk, block_num + bitmap_block_idx, d_bmap) < 0)
        return -ERR;

    SetBit(d_bmap[offset], bit_pos);
    write_block(fs.disk, block_num + bitmap_block_idx, d_bmap);

    return entry;
}

uint32_t get_free_inode(int *ret)
{
    return get_free_entry(fs.inode_bmap, fs.len_inodes_bmap, fs.sb.inode_bitmap_block_idx, ret);
}

uint32_t get_free_data_block(int *ret)
{
    return get_free_entry(fs.db_bmap, fs.len_db_bmap, fs.sb.data_block_bitmap_idx, ret);
}

int create_file()
{
    if (fs.disk == -1)
        return -ERR;

    int ret;
    uint32_t fin = get_free_inode(&ret);
    if (ret < 0)
        return -ERR;

    uint32_t fin_block = fin / INODES_PER_BLOCK;
    uint32_t fin_off = fin % INODES_PER_BLOCK;

    Block block;
    if (read_block(fs.disk, fin_block + fs.sb.inode_block_idx, &block) < 0)
        return -ERR;
    
    block.in[fin_off].valid = 1;
    block.in[fin_off].size = 0;

    if (write_block(fs.disk, fin_block + fs.sb.inode_block_idx, &block) < 0)
        return -ERR;

    return fin;
}

int load_inode(int inumber, inode* in)
{
    Block block;
    if (read_block(fs.disk, inumber / INODES_PER_BLOCK + fs.sb.inode_block_idx, &block) < 0)
        return -ERR;

    *in = block.in[inumber % INODES_PER_BLOCK];
    if (!in->valid)
        return -ERR;
    return 0;
}

int save_inode(int inumber, inode* in)
{
    Block block;
    if (read_block(fs.disk, inumber / INODES_PER_BLOCK + fs.sb.inode_block_idx, &block) < 0)
        return -ERR;

    block.in[inumber % INODES_PER_BLOCK] = *in;

    return write_block(fs.disk, inumber / INODES_PER_BLOCK + fs.sb.inode_block_idx, &block);
}

int inumber_check(int inumber)
{
    if (fs.disk == -1 || inumber < 0 || inumber >= fs.sb.inodes  < 0) {
        printf("Invalid inumber %d\n", inumber);
        return -ERR;
    }
    return 0;
}

int read_i(int inumber, char *data, int length, int offset)
{
    inode in;
    if ((inumber_check(inumber) < 0) || (load_inode(inumber, &in) < 0))
        return -ERR;

    if ((offset < 0) || (offset >= in.size))
        return -ERR;

    length = min(length + offset, in.size);
    uint32_t req_dblocks = _ceil(length, BLOCKSIZE), cur_blocks = offset / BLOCKSIZE;

    int tmpZ = -1, ok_ind = -1;
    uint32_t ind_ptrs[PTRS_PER_BLOCK];

    while (cur_blocks < req_dblocks) {
        int tmp;
        if (cur_blocks < 5) 
            tmp = in.direct[cur_blocks];
        else if (cur_blocks < (5 + PTRS_PER_BLOCK)) {
            if (ok_ind == -1) {
                ok_ind = 1;
                read_block(fs.disk, in.indirect + fs.sb.data_block_idx, ind_ptrs);
            }
            tmp = ind_ptrs[cur_blocks - 5];
        } else {
            printf("Block val out of range\n");
            return -ERR;
        }
        char block[BLOCKSIZE];
        read_block(fs.disk, tmp + fs.sb.data_block_idx, block);

        if (tmpZ == -1) {
            tmpZ = min((cur_blocks + 1) * BLOCK_SIZE, length) - offset;
            strncpy(data, block + (offset - cur_blocks * BLOCKSIZE), tmpZ);
        } else {
            tmp = min(BLOCK_SIZE, length - cur_blocks * BLOCK_SIZE);
            strncpy(data + tmpZ, block, tmp);
            tmpZ += tmp;
        }
        cur_blocks ++;
    }

    return tmpZ;
}

int write_i(int inumber, char *data, int length, int offset)
{
    inode in;
    if ((inumber_check(inumber) < 0) || (load_inode(inumber, &in) < 0))
        return -ERR;

    if (offset < 0)
        return -ERR;

    length += offset;

    uint32_t req_dblocks = _ceil(length, BLOCKSIZE), cur_blocks = offset / BLOCKSIZE, now_dblocks = _ceil(in.size, BLOCKSIZE);
   // if offset > in.size, allocate upto cur_blocks 
    int tmpZ = -1, ok_ind = -1;
    uint32_t ind[PTRS_PER_BLOCK];

    while (cur_blocks < req_dblocks) {
        int tmp, ret = 1;
        uint32_t nen;
        if (cur_blocks >= now_dblocks) {
            nen = get_free_data_block(&ret);
            if (ret < 0) return -ERR;
        }

        if (cur_blocks < 5) {
            if (!ret) {
                now_dblocks++;
                in.direct[cur_blocks] = nen;
            }
            tmp = in.direct[cur_blocks];
        } else if (cur_blocks < (5 + PTRS_PER_BLOCK)) {
            if (cur_blocks == 5 && !ret) {
                in.indirect = nen;
                nen = get_free_data_block(&ret);
                if (ret < 0) return -ERR;
            }

            if (!ret) {                
                now_dblocks ++;
                uint32_t ptrs[PTRS_PER_BLOCK];
                read_block(fs.disk, in.indirect + fs.sb.data_block_idx, ptrs);
                ptrs[cur_blocks - 5] = nen;
                write_block(fs.disk, in.indirect + fs.sb.data_block_idx, ptrs);
            }
            tmp = nen;
        } else {
            return -ERR;
        }

        char block[BLOCKSIZE];
        read_block(fs.disk, tmp + fs.sb.data_block_idx, block);
        
        if (tmpZ == -1) {
            tmpZ = min((cur_blocks + 1) * BLOCK_SIZE, length) - offset;
            strncpy(block + (offset - cur_blocks * BLOCKSIZE), data, tmpZ);
        } else {
            tmp = min(BLOCK_SIZE, length - cur_blocks * BLOCK_SIZE);
            strncpy(block, data + tmpZ, tmp);
            tmpZ += tmp;
        }
        
        write_block(fs.disk, tmp + fs.sb.data_block_idx, block);

        cur_blocks ++;
    }
    in.size = max(in.size, length);

    save_inode(inumber, &in);
    return tmpZ;
}

int remove_file(int inumber)
{
    inode in;
    if ((inumber_check(inumber) < 0) || (load_inode(inumber, &in) < 0))
        return -ERR;
    uint32_t req_dblocks = _ceil(in.size, BLOCKSIZE), cur_blocks = 0;
    int ok_ind = -1;
    uint32_t ind_ptrs[PTRS_PER_BLOCK];

    while (cur_blocks < req_dblocks) {
        int tmp;
        if (cur_blocks < 5) 
            tmp = in.direct[cur_blocks];
        else if (cur_blocks < (5 + PTRS_PER_BLOCK)) {
            if (ok_ind == -1) {
                ok_ind = 1;
                read_block(fs.disk, in.indirect + fs.sb.data_block_idx, ind_ptrs);
            }
            tmp = ind_ptrs[cur_blocks - 5];
            if (cur_blocks == (4 + PTRS_PER_BLOCK)) {
                // remove block for indirect ptr once all ind_ptrs are removed
                if (clear_data_block_entry(in.indirect) < 0) 
                    return -ERR;
            }

        } else {
            printf("Block val out of range\n");
            return -ERR;
        }
        if (clear_data_block_entry(tmp) < 0) 
            return -ERR;
        cur_blocks ++;
    }

    in.valid = 0;
    in.size = 0;

    if (save_inode(inumber, &in) < 0)
        return -ERR;

    if (clear_inode_bitmap_entry(inumber) < 0)
        return -ERR;
    return 0;
}

int stats(int inumber)
{
    inode in;
    if ((inumber_check(inumber) < 0) || (load_inode(inumber, &in) < 0))
        return -ERR;

    printf("Stats of inode %d\n", inumber);
    printf("Logical size = %u\n", in.size);

    uint32_t num_blocks = _ceil(in.size, BLOCKSIZE);
    uint32_t dir = (num_blocks > 5) ? 5 : num_blocks;

    printf("Number of data blocks for the inode = %u\n", num_blocks); 
    printf("Number of direct pointers used = %u\n", dir);
    printf("Number of indirect pointers used = %u\n", num_blocks - dir);

    return 0;
}

char * clean_path(char *path)
{
    int n = strlen(path);
    if (n == 0) return path;
    if (path[0] == '/')
        path++;
    if (n > 1 && path[n - 1] == '/')
        path[n - 1] = '\0';

    return path;
}

int count_slashes(char *path)
{
    int cnt = 0, i, n = strlen(path);
    for (i = 0; i < n; i++) 
        if (path[i] == '/') 
            cnt++;

    return cnt;
}

char** parse_path(char* path, int *cnt)
{
    path = clean_path(path);
    *cnt = count_slashes(path) + 1;
    int i;
    // printf("Parsing path = %s\n", path);

    char** path_parts = (char **) malloc(sizeof(char *) * *cnt);
    for (i = 0; i < *cnt; i++)
        path_parts[i] = (char *) malloc(sizeof(char) * MAX_LEN);

    int j = 0, n = strlen(path);
    for (i = 0; i < *cnt; i++) {
        int k = 0;
        while (j < n && path[j] != '/')
            path_parts[i][k++] = path[j++];
        path_parts[i][k] = '\0';

        j++;
        
        // printf("path i = %d, val = %s\n", i, path_parts[i]);
    }

    return path_parts;
}

int get_dirblock(int inum, int off, inode *in, int by_force)
{
    int num_ls = in->size / sizeof(dir_entry);
    if (num_ls <= off && by_force == 0)
        return -ERR;

    if (off < 5 * DIRS_PER_BLOCK) {
       int block_n = off / DIRS_PER_BLOCK, offset = off % DIRS_PER_BLOCK; //
       int ret;

       if (by_force == 1 && (offset == 0)) { // first entry of the block
            int nen = get_free_data_block(&ret);
            if (ret < 0)
                return -ERR;
            in->direct[block_n] = nen;

            save_inode(inum, in);
       }

       return in->direct[block_n];
    }
    return -ERR;
}


dir_entry* read_dir(int inum, int readp)
{
    inode in;
    load_inode(inum, &in);

    int block_n = get_dirblock(inum, readp, &in, 0);

    if (block_n < 0)
        return NULL;

    Block block;
    if (read_block(fs.disk, block_n + fs.sb.data_block_idx, &block) < 0)
        return NULL;

    dir_entry* d = (dir_entry *) malloc(sizeof(dir_entry));
    *d = block.dir[readp % DIRS_PER_BLOCK];
    
    return d;
}

dir_entry* search_dir(int inum, char* fname)
{
    int readp = 0;
    dir_entry *entry = read_dir(inum, readp++);
    
    while (entry) {
        if (strcmp(fname, entry->filename) == 0)
            return entry;

        free(entry);
        entry = read_dir(inum, readp++);
    }

    // printf("%s not found in directory\n", fname);
    return NULL;
}

int get_parent_inode_h(char** dir_parts, int num_parts)
{
    int cwd_inode = root_inode;
    int i;
    for (i = 0; i < num_parts - 1; i++) {
        dir_entry *d = search_dir(cwd_inode, dir_parts[i]);
        if (d) {
            cwd_inode = d->inum;
            free(d);
        } else {
            printf("Invalid path: Can't find %s\n", dir_parts[i]);
            return -ERR;
        }

    }

    return cwd_inode;
}

int add_direntry(int inum, char *filename, int ty)
{
    inode in;
    load_inode(inum, &in);

    int num_ls = in.size / sizeof(dir_entry);
    
    int nb = get_dirblock(inum, num_ls, &in, 1);

    if (nb < 0) 
        return -ERR;
    
    Block bl;
    if (read_block(fs.disk, fs.sb.data_block_idx + nb, &bl)) 
        return -ERR;

    dir_entry d;
    d.flags |= 1;
    if (ty == DIR)
        d.flags |= 2;

    d.len = strlen(filename);
    d.inum = create_file();
    strcpy(d.filename, filename);

    bl.dir[num_ls % DIRS_PER_BLOCK] = d;

    if (write_block(fs.disk, fs.sb.data_block_idx + nb, &bl) < 0) 
        return -ERR;

    in.size += sizeof(dir_entry);
    save_inode(inum, &in);

    return d.inum;
}

int get_parent_inode(char *path, char** basename)
{
    int num_parts;
    char** dir_parts = parse_path(path, &num_parts);
    if (num_parts == 0) {
        printf("Not a valid path\n");
        return -ERR;
    }

    int par_in = get_parent_inode_h(dir_parts, num_parts);
    if (par_in < 0)
        return -ERR;
    *basename = (char *)malloc(sizeof(char) * MAX_LEN);
    int i;
    strcpy(*basename, dir_parts[num_parts - 1]);

    for (i = 0; i < num_parts; i++)
        free(dir_parts[i]);

    return par_in;
}

int is_file(char *path)
{
    char* basename;
    int par_in = get_parent_inode(path, &basename);
    dir_entry* d = search_dir(par_in, basename);
    if (!d)
        return -1;
    return (d->flags >> 1) & 1;
}

void list_dir(int inum)
{
    printf("ls for inode = %d\n", inum);
    int readp = 0;
    dir_entry *entry = read_dir(inum, readp++);

    while (entry) {
        printf("%s\n", entry->filename);
        free(entry);
        entry = read_dir(inum, readp++);
    }
    printf("LS END\n");
}

int open_file(char *filepath)
{
    char *basename;
    int par_in = get_parent_inode(filepath, &basename);
    if (par_in < 0)
        return -ERR;
    return add_direntry(par_in, basename, REG);
}

int create_dir(char *dirpath)
{
    char *basename;
    int par_in = get_parent_inode(dirpath, &basename);
    if (par_in < 0)
        return -ERR;

    int ret = add_direntry(par_in, basename, DIR);
    // list_dir(par_in);

    return ret;
}

int read_file(char *filepath, char *data, int length, int offset)
{
    char *basename;
    int par_in = get_parent_inode(filepath, &basename);
    if (par_in < 0) 
        return -ERR;
    dir_entry *d = search_dir(par_in, basename);
    if (!d) 
        return -ERR;

    int inum = d->inum;
    free(d);

    return read_i(inum, data, length, offset);
}

int write_file(char *filepath, char *data, int length, int offset)
{
    char* basename;
    int par_in = get_parent_inode(filepath, &basename);
    if (par_in < 0) return -ERR;
    
    dir_entry *d = search_dir(par_in, basename);
    int inum;
    if (!d) {
        inum = open_file(filepath);
        if (inum < 0) 
            return -ERR;
        printf("Creating file = %s\n", filepath);
    } else 
        inum = d->inum;

    free(d);
    return write_i(inum, data, length, offset);
}

int get_offset(int inum, char *filename)
{
    int readp = 0;
    dir_entry *entry = read_dir(inum, readp++);
    int flen = strlen(filename);

    while (entry) {
        int len = min(entry->len, flen);
        if (strncmp(filename, entry->filename, len) == 0)
            return readp - 1;

        free(entry);
        entry = read_dir(inum, readp++);
    }

    return -ERR;
}

int rm_direntry(int inum, char *filename)
{
    dir_entry* entry = search_dir(inum, filename);
    if (!entry)
        return -ERR;

    int offset = get_offset(inum, filename);

    if (!TestBit(entry->flags, 1)) // regular file
        remove_file(entry->inum);
    else {
        // recursively delete all
        int readp = 0;
        dir_entry *d = read_dir(entry->inum, readp++);
        
        while (d) {
            if (rm_direntry(entry->inum, d->filename) < 0) {
                printf("Deleting dir failed.. cant delete %s\n", d->filename);
                return -ERR;
            }
            free(d);
            d = read_dir(entry->inum, readp++);
        }

        inode in2;
        load_inode(entry->inum, &in2);

        int i;
        for (i = 0; i < 5; i++)
            clear_data_block_entry(in2.direct[i]);

        clear_inode_bitmap_entry(entry->inum);
    }
        
    inode in;
    load_inode(inum, &in);

    int num_ls = in.size / sizeof(dir_entry);

    dir_entry* lst = NULL;
    if (num_ls > 1)
        lst = read_dir(inum, num_ls - 1);

    in.size -= sizeof(dir_entry);
    save_inode(inum, &in);

    if (!lst) 
        return 0;
    
    int nb = get_dirblock(inum, offset, &in, 0);

    Block bl;
    if (read_block(fs.disk, fs.sb.data_block_idx + nb, &bl) < 0) 
        return -ERR;
    
    bl.dir[offset % DIRS_PER_BLOCK] = *lst;
    
    if (write_block(fs.disk, fs.sb.data_block_idx + nb, &bl) < 0) 
        return -ERR;
    
    free(lst);
    return 0;
}

int remove_dir(char *dirpath)
{
    char *basename;
    int par_in = get_parent_inode(dirpath, &basename);
    if (par_in < 0)
        return -ERR;

    int ret = rm_direntry(par_in, basename);    
    // list_dir(par_in);

    return ret;
}

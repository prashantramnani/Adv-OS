#include <stdint.h>

const static uint32_t MAGIC = 12345;
const static float INODE_FRAC = 0.1;
const static uint32_t root_inode = 0;

#define BLOCK_SIZE 4096
#define PTRS_PER_BLOCK 1024
#define INODES_PER_BLOCK 128
#define MAX_LEN 250
#define DIRS_PER_BLOCK 16

typedef enum {
   REG,
   DIR
} file_ty;

typedef struct inode {
	uint32_t valid; // 0 if invalid
	uint32_t size; // logical size of the file
	uint32_t direct[5]; // direct data block pointer
	uint32_t indirect; // indirect pointer
} inode;

typedef struct super_block {
	uint32_t magic_number;	// File system magic number
	uint32_t blocks;	// Number of blocks in file system (except super block)

	uint32_t inode_blocks;	// Number of blocks reserved for inodes == 10% of Blocks
	uint32_t inodes;	// Number of inodes in file system == length of inode bit map
	uint32_t inode_bitmap_block_idx;  // Block Number of the first inode bit map block
	uint32_t inode_block_idx;	// Block Number of the first inode block

	uint32_t data_block_bitmap_idx;	// Block number of the first data bitmap block
	uint32_t data_block_idx;	// Block number of the first data block
	uint32_t data_blocks;  // Number of blocks reserved as data blocks
} super_block;

typedef struct {
    uint8_t flags;  // Valid bit = 0th bit, Type bit = 1th bit
    char filename[MAX_LEN];
    uint8_t len; // length of file name
    uint32_t inum; // inode num of file
} dir_entry;

typedef union Block {
    super_block sb;
    uint32_t ptrs[PTRS_PER_BLOCK];
    inode in[INODES_PER_BLOCK];
    uint32_t bmap[PTRS_PER_BLOCK];
    char db[BLOCK_SIZE];
    dir_entry dir[DIRS_PER_BLOCK];
} Block;

typedef enum block_ty{
    SUPERBLOCK,
    INODE_BM,
    DATABLOCK_BM,
    INODE,
    DATABLOCK,
    UNDEF
} block_ty;

typedef struct mem_fs{
    super_block sb;
    uint32_t* inode_bmap;
    uint32_t* db_bmap;

    uint32_t len_inodes_bmap;
    uint32_t len_db_bmap;
    int disk; // fd of disk
} mem_fs;

static mem_fs fs = {.disk = -1}; // only one disk at a time

int format(int disk);

int mount_sfs(int disk);

int create_file();

int remove_file(int inumber);

// gcc -c -g sfs.c `pkg-config fuse --cflags --libs`
// In file included from /usr/include/features.h:367:0,
//                  from /usr/include/stdint.h:25,
//                  from /usr/lib/gcc/x86_64-linux-gnu/5/include/stdint.h:9,
//                  from sfs.h:1,
//                  from sfs.c:1:
// /usr/include/x86_64-linux-gnu/sys/stat.h:216:12: error: conflicting types for ‘stat’
//  extern int __REDIRECT_NTH (stat, (const char *__restrict __file,
//             ^
// In file included from sfs.c:1:0:
// sfs.h:84:5: note: previous declaration of ‘stat’ was here
//  int stat(int inumber);
//      ^
// Makefile:6: recipe for target 'sfs.o' failed
// had to change the function name as there exists already a function named 
// stat and i was getting a compile time error because of that
int stats(int inumber);

int read_i(int inumber, char *data, int length, int offset);

int write_i(int inumber, char *data, int length, int offset);

int is_file(char *path); // return 1/0 depending on dir/file

// returns file descriptor.
int open_file(char *filepath); // A function to create a file. create_file doesn't take any filepath as argument.

int read_file(char *filepath, char *data, int length, int offset);
int write_file(char *filepath, char *data, int length, int offset);
int create_dir(char *dirpath); // assumes cwd is always at root.
int remove_dir(char *dirpath);

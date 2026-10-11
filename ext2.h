#ifndef EXT2
#define EXT2

#include <stdint.h>

#define EXT2_SIGNATURE 0xEF53
#define EXT2_SB_OFFSET 1024
#define EXT2_SB_MIN_SIZE 1024
#define EXT2_MAX_BLOCK_SIZE 4096

#define SWAP16(x) __builtin_bswap16(x)
#define SWAP32(x) __builtin_bswap32(x)

enum dir_type
{
    UNKNOWN_dt,
    FILE_dt,
    DIR_dt,
    CHAR_DEV_dt,
    BLOCK_DEV_dt,
    FIFO_dt,
    SOCKET_dt,
    SOFT_LINK_dt,
};

enum inode_type
{
    FIFO_it = 0x1000,
    CHAR_DEV_it = 0x2000,
    DIR_it = 0x4000,
    BLOCK_DEV_it = 0x6000,
    FILE_it = 0x8000,
    SOFT_LINK_it = 0xA000,
    SOCKET_it = 0xC000,
};

#pragma pack(push, 1)
typedef struct
{
    uint32_t inodes_total;
    uint32_t blocks_total;
    uint32_t blocks_s_user;
    uint32_t blocks_free;
    uint32_t inodes_free;
    uint32_t start_block_num;
    uint32_t log_block_size;
    uint32_t log_frag_size;
    uint32_t blocks_in_group;
    uint32_t frags_in_group;
    uint32_t inodes_in_group;
    uint32_t last_mount_time;
    uint32_t last_written_time;
    uint16_t mount_count_since_check;
    uint16_t mount_count_before_check;
    uint16_t ext2_signature;
    uint16_t fs_state;
    uint16_t err_method;
    uint16_t version_minor;
    uint32_t last_check_time;
    uint32_t check_interval;
    uint32_t os_id;
    uint32_t version_major;
    uint16_t user_id;
    uint16_t group_id;
    uint32_t fst_inode_no_reserv;
    uint16_t inode_size;
    uint16_t block_group_if_copy;
    uint32_t features_opt;
    uint32_t features_req;
    uint32_t RO_if_no_support;
    uint8_t fs_id[16];
    uint8_t volume_name[16];
    uint8_t last_mounted_path[64];
    uint32_t comp_alg;
    uint8_t blocks_prealloc_files;
    uint8_t blocks_prealloc_dirs;
    uint16_t gap_1;
    uint8_t journal_id[16];
    uint32_t journal_inode;
    uint32_t journal_dev;
    uint32_t orphan_inode_list_head;
    uint8_t gap_2[788];
} superblock;

typedef struct
{
    uint32_t block_bitmap_addr;
    uint32_t inode_bitmap_addr;
    uint32_t inode_table_start;
    uint16_t blocks_free;
    uint16_t inodes_free;
    uint16_t dirs_in_group;
    uint8_t gap[14];
} block_group;

typedef struct
{
    uint32_t inode;
    uint16_t entry_size;
    uint8_t name_length;
    uint8_t type;
    uint8_t name[];
} dir;

typedef struct
{
    uint16_t type_perms;
    uint16_t user_id;
    uint32_t size;
    uint32_t last_access_time;
    uint32_t creation_time;
    uint32_t last_mod_time;
    uint32_t deletion_time;
    uint16_t group_id;
    uint16_t hard_link_count;
    uint32_t disc_sector_count;
    uint32_t flags;
    uint32_t os_spec_val_1;
    uint32_t block_ptrs[12];
    uint32_t block_ptr_indir[3];
    uint32_t gen_num;
    uint32_t file_acl;
    uint32_t dir_acl;
    uint32_t frag_addr;
    uint8_t os_spec_val_2[12];
} inode;

#pragma pack(pop)

typedef struct
{
    int fd;
    uint8_t is_be;
    uint32_t block_size;
    uint32_t inode_size;
    uint32_t inodes_in_group;
    uint32_t blocks_in_group;
    uint32_t block_groups_total;
} fs_info;

int32_t open_fs(fs_info *fs, const char *dev);
int32_t read_superblock(fs_info *fs, superblock *sb);
int32_t read_inode(fs_info *fs, uint32_t num, inode *i);
int32_t read_block(fs_info *fs, uint32_t num, void *b);
int32_t read_data(fs_info *fs, inode *inode, int32_t (*callback)(uint32_t, void *), void *data);
int32_t read_indirect_blocks(fs_info *fs, uint32_t block, int level, int32_t (*callback)(uint32_t, void *), void *data, uint32_t *processed, uint32_t needed);

uint8_t is_be();
uint16_t fix_end_16(uint16_t val);
uint32_t fix_end_32(uint32_t val);

#endif
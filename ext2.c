#include "ext2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define DIV_ROUND_UP(a, b) (((a) + (b) - 1) / (b))

uint64_t pread_all(int fd, void *buf, uint64_t len, off_t offset)
{
    uint64_t total = 0;
    ssize_t counter = 0;

    while (total < len)
    {
        counter = pread(fd, (uint8_t *)buf + total, len - total, offset + total);
        if (counter < 0)
            return -1;
        if (!counter)
            break;
        total += counter;
    };
    return total;
};

int32_t open_fs(fs_info *fs, const char *dev)
{
    superblock sb;

    if (!fs || !dev)
    {
        fprintf(stderr, "open_fs: null pointer(s)\n");
        return -1;
    }

    if ((fs->fd = open(dev, O_RDONLY)) < 0)
    {
        fprintf(stderr, "open_fs: unable to open device\n");
        return -1;
    }

    fs->is_be = is_be();

    if (read_superblock(fs, &sb) < 0)
    {
        fprintf(stderr, "open_fs: unable to read superblock\n");
        close(fs->fd);
        return -1;
    }

    fs->block_size = EXT2_SB_MIN_SIZE << fix_end_32(sb.log_block_size);
    fs->inode_size = fix_end_16(sb.inode_size);
    fs->blocks_in_group = fix_end_32(sb.blocks_in_group);
    fs->inodes_in_group = fix_end_32(sb.inodes_in_group);
    fs->block_groups_total = DIV_ROUND_UP(fix_end_32(sb.blocks_total), fs->blocks_in_group);
    return 0;
}

int32_t read_superblock(fs_info *fs, superblock *sb)
{
    if (!fs || !sb)
    {
        fprintf(stderr, "read_superblock: null pointer(s)\n");
        return -1;
    }

    if (pread_all(fs->fd, sb, sizeof(superblock), EXT2_SB_OFFSET) != sizeof(superblock))
    {
        fprintf(stderr, "read_superblock: unable to read superblock\n");
        return -1;
    }

    if (fix_end_16(sb->ext2_signature) != EXT2_SIGNATURE)
    {
        fprintf(stderr, "read_superblock: not ext2 fs\n");
        return -1;
    }

    if (fix_end_32(sb->log_block_size) > 2)
    {
        fprintf(stderr, "read_superblock: unsupported block size\n");
        return -1;
    }

    uint16_t inode_size = fix_end_16(sb->inode_size);
    if (inode_size != 128 && inode_size != 256)
    {
        fprintf(stderr, "read_superblock: unsupported inode size\n");
        return -1;
    }

    if (!fix_end_32(sb->blocks_in_group))
    {
        fprintf(stderr, "read_superblock: invalid block count in group\n");
        return -1;
    }

    if (!fix_end_32(sb->inodes_in_group))
    {
        fprintf(stderr, "read_superblock: invalid inode count in group\n");
        return -1;
    }

    if (!fix_end_32(sb->blocks_total))
    {
        fprintf(stderr, "read_superblock: invalid total block count\n");
        return -1;
    }

    if (!fix_end_32(sb->inodes_total))
    {
        fprintf(stderr, "read_superblock: invalid total inode count\n");
        return -1;
    }

    return 0;
}

int32_t read_inode(fs_info *fs, uint32_t num, inode *i)
{
    uint32_t group, index, desc_table_block, inode_table;
    off_t bg_offset, inode_offset;
    block_group bg;

    if (!fs || !i || !num)
    {
        fprintf(stderr, "read_inode: null pointer(s) or inode number\n");
        return -1;
    }

    group = (num - 1) / fs->inodes_in_group;
    index = (num - 1) % fs->inodes_in_group;
    if (group >= fs->block_groups_total)
    {
        fprintf(stderr, "read_inode: group out of range\n");
        return -1;
    }

    desc_table_block = (fs->block_size == EXT2_SB_MIN_SIZE) ? 2 : 1;
    bg_offset = desc_table_block * fs->block_size + group * sizeof(block_group);
    if (pread_all(fs->fd, &bg, sizeof(block_group), bg_offset) != sizeof(block_group))
    {
        fprintf(stderr, "read_inode: cannot read group descriptor\n");
        return -1;
    }

    inode_table = fix_end_32(bg.inode_table_start);
    inode_offset = (off_t)inode_table * fs->block_size + index * fs->inode_size;
    if (pread_all(fs->fd, i, fs->inode_size, inode_offset) != fs->inode_size)
    {
        fprintf(stderr, "read_inode: cannot read inode\n");
        return -1;
    }

    return 0;
}

int32_t read_block(fs_info *fs, uint32_t num, void *b)
{
    if (!fs || !b)
    {
        fprintf(stderr, "read_block: null pointer(s)\n");
        return -1;
    }

    off_t offset = (off_t)num * fs->block_size;
    if (pread_all(fs->fd, b, fs->block_size, offset) != (ssize_t)fs->block_size)
    {
        fprintf(stderr, "read_block: unable to read block\n");
        return -1;
    }

    return 0;
}

int32_t read_indirect_blocks(fs_info *fs, uint32_t block, int level, int32_t (*callback)(uint32_t, void *), void *data, uint32_t *processed, uint32_t needed)
{
    uint32_t blk, count;
    uint32_t ptrs_per_block = fs->block_size / sizeof(uint32_t);
    uint32_t block_data[EXT2_MAX_BLOCK_SIZE / sizeof(uint32_t)];

    if (!block)
    {
        count = 1;
        for (int i = 0; i < level; i++)
            count *= ptrs_per_block;
        for (uint32_t i = 0; i < count && *processed < needed; i++)
        {
            if (callback(0, data) < 0)
            {
                fprintf(stderr, "read_indirect_blocks: error working with empty block\n");
                return -1;
            }
            (*processed)++;
        }
        return 0;
    };

    if (read_block(fs, block, block_data) < 0)
    {
        fprintf(stderr, "read_indirect_blocks: unable to read block\n");
        return -1;
    }

    for (uint32_t i = 0; i < ptrs_per_block; i++)
    {
        blk = fix_end_32(block_data[i]);
        if (level == 1)
        {
            if (callback(blk, data) < 0)
            {
                fprintf(stderr, "read_indirect_blocks: unable to read indirect blocks\n");
                return -1;
            }
            (*processed)++;
        }
        else if (read_indirect_blocks(fs, blk, level - 1, callback, data, processed, needed) < 0)
            return -1;
    }
    return 0;
}

int32_t read_data(fs_info *fs, inode *inode, int32_t (*callback)(uint32_t, void *), void *data)
{
    uint32_t size, needed, processed, blk;

    if (!fs || !inode || !callback || !data)
    {
        fprintf(stderr, "read_data: null pointer(s)\n");
        return -1;
    }

    size = fix_end_32(inode->size);
    needed = (size + fs->block_size - 1) / fs->block_size;
    processed = 0;
    for (int i = 0; i < 12; i++)
    {
        blk = fix_end_32(inode->block_ptrs[i]);
        if (callback(blk, data) < 0)
        {
            fprintf(stderr, "read_data: unable to read block\n");
            return -1;
        }
        processed++;
    }

    for (int i = 0; i < 3; i++)
    {
        blk = fix_end_32(inode->block_ptr_indir[i]);
        if (read_indirect_blocks(fs, blk, i + 1, callback, data, &processed, needed) < 0)
        {
            fprintf(stderr, "read_data: unable to read indirect block\n");
            return -1;
        }
    }
    
    return 0;
}

uint8_t is_be()
{
    uint16_t test = 0x0001;
    return *(uint8_t *)&test == 0x00;
};

uint16_t fix_end_16(uint16_t val)
{
    return is_be() ? SWAP16(val) : val;
};

uint32_t fix_end_32(uint32_t val)
{
    return is_be() ? SWAP32(val) : val;
};

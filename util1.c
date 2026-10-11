#include "ext2.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

const char *get_inode_type(uint16_t type)
{
    switch (type & 0xF000)
    {
    case FIFO_it:
        return "FIFO";
    case CHAR_DEV_it:
        return "character device";
    case DIR_it:
        return "directory";
    case BLOCK_DEV_it:
        return "block device";
    case FILE_it:
        return "file";
    case SOFT_LINK_it:
        return "soft link";
    case SOCKET_it:
        return "socket";
    default:
        return "unknown";
    }
}

void print_perms(uint16_t perms)
{
    printf("\t other: ");
    putchar((perms & 0x001) ? 'x' : '_');
    putchar((perms & 0x002) ? 'w' : '_');
    putchar((perms & 0x004) ? 'r' : '_');
    printf("\t group: ");
    putchar((perms & 0x008) ? 'x' : '_');
    putchar((perms & 0x010) ? 'w' : '_');
    putchar((perms & 0x020) ? 'r' : '_');
    printf("\t user: ");
    putchar((perms & 0x040) ? 'x' : '_');
    putchar((perms & 0x080) ? 'w' : '_');
    putchar((perms & 0x100) ? 'r' : '_');
    if (perms & 0x200)
        printf("\t sticky bit");
    if (perms & 0x400)
        printf("\t set group ID");
    if (perms & 0x800)
        printf("\t set user ID");
    printf("\n");
}

void print_flags(uint32_t flags)
{
    printf("flags: %d", flags);
    if (flags & 0x00000001)
        printf("\t secure deletion,");
    if (flags & 0x00000002)
        printf("\t copy on delete,");
    if (flags & 0x00000004)
        printf("\t compressed,");
    if (flags & 0x00000008)
        printf("\t synchronous updates,");
    if (flags & 0x00000010)
        printf("\t immutable,");
    if (flags & 0x00000020)
        printf("\t append only,");
    if (flags & 0x00000040)
        printf("\t no dump,");
    if (flags & 0x00000080)
        printf("\t no change of last access time,");
    if (flags & 0x00010000)
        printf("\t hash indexed,");
    if (flags & 0x00020000)
        printf("\t AFS directory,");
    if (flags & 0x00040000)
        printf("\t journal data,");
    printf("\n");
}

void format_time(uint32_t timestamp, char *buf, size_t size)
{
    time_t t = (time_t)timestamp;
    struct tm *tm = localtime(&t);
    if (tm)
        strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm);
    else
        snprintf(buf, size, "%u", timestamp);
}

void print_rec(fs_info *fs, uint32_t blk, int lvl)
{
    if (!blk || !fs || lvl < 0)
        return;
    uint32_t child;
    uint32_t blk_data[EXT2_MAX_BLOCK_SIZE / sizeof(uint32_t)];
    uint32_t ptrs = fs->block_size / sizeof(uint32_t);
    if (read_block(fs, blk, blk_data) < 0)
        return;
    for (uint32_t i = 0; i < ptrs; i++)
    {
        child = fix_end_32(blk_data[i]);
        if (!child)
            continue;
        if (!lvl)
            printf("\tdata block: %u\n", child);
        else
        {
            printf("\tindirect block %u (level %d):\n", child, lvl);
            print_rec(fs, child, lvl - 1);
        }
    }
}

int main(int argc, char *argv[])
{
    fs_info fs;
    int num;
    inode ind;

    if (argc != 3)
    {
        fprintf(stderr, "invalid argument format\n");
        return 1;
    }
    
    if (open_fs(&fs, argv[1]) < 0)
    {
        fprintf(stderr, "unable to open fs\n");
        return 1;
    };

    num = atoi(argv[2]);
    if (num < 1)
    {
        fprintf(stderr, "invalid inode number\n");
        return 1;
    }
    
    if (read_inode(&fs, num, &ind) < 0)
    {
        fprintf(stderr, "unable to read inode\n");
        return 1;
    }

    printf("inode: %u\n", num);
    printf("type: %s\n", get_inode_type(fix_end_16(ind.type_perms)));
    printf("permissions: ");
    print_perms(fix_end_16(ind.type_perms));
    printf("user ID: %u\n", fix_end_16(ind.user_id));
    printf("size: %u bytes\n", fix_end_32(ind.size));

    char buf[64];
    format_time(fix_end_32(ind.last_access_time), buf, sizeof(buf));
    printf("last access time: %s\n", buf);
    format_time(fix_end_32(ind.creation_time), buf, sizeof(buf));
    printf("creation time: %s\n", buf);
    format_time(fix_end_32(ind.last_mod_time), buf, sizeof(buf));
    printf("last modified time: %s\n", buf);
    format_time(fix_end_32(ind.deletion_time), buf, sizeof(buf));
    printf("deletion time: %s\n", buf);

    printf("group ID: %u\n", fix_end_16(ind.group_id));
    printf("hard link count: %u\n", fix_end_16(ind.hard_link_count));
    printf("disc sector count: %u\n", fix_end_32(ind.disc_sector_count));
    print_flags(fix_end_32(ind.flags));

    uint32_t blk;
    printf("direct blocks:");
    for (int i = 0; i < 12; i++)
    {
        blk = fix_end_32(ind.block_ptrs[i]);
        if (blk)
            printf(" %u", blk);
    }
    printf("\n");

    const char *labels[] = {"single indirect", "double indirect", "triple indirect"};
    for (int i = 0; i < 3; i++)
    {
        blk = fix_end_32(ind.block_ptr_indir[i]);
        if (blk)
        {
            printf("%s: %u\n", labels[i], blk);
            print_rec(&fs, blk, i + 1);
        }
    }
    printf("\n");

    printf("generation number: %u\n", fix_end_32(ind.gen_num));
    printf("file ACL: %u\n", fix_end_32(ind.file_acl));
    printf("directory ACL: %u\n", fix_end_32(ind.dir_acl));
    printf("fragment address: %u\n", fix_end_32(ind.frag_addr));

    close(fs.fd);
    return 0;
}
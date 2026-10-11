#include "ext2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *get_dir_type(uint8_t type)
{
    switch (type)
    {
    case UNKNOWN_dt:
        return "unknown";
    case FILE_dt:
        return "file";
    case DIR_dt:
        return "directory";
    case CHAR_DEV_dt:
        return "character device";
    case BLOCK_DEV_dt:
        return "block device";
    case FIFO_dt:
        return "FIFO";
    case SOCKET_dt:
        return "socket";
    case SOFT_LINK_dt:
        return "soft link";
    default:
        return "unknown";
    }
}

int main(void)
{
    uint8_t buf[EXT2_MAX_BLOCK_SIZE];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    if (!n)
    {
        fprintf(stderr, "no data on stdin\n");
        return 1;
    }

    uint32_t pos = 0;
    dir *entry;
    char name[256];

    printf("%-10s %-6s %s\n", "Inode", "Type", "Name");
    printf("%-10s %-6s %s\n", "----------", "------", "----");

    while (pos + sizeof(dir) <= n)
    {
        entry = (dir *)(buf + pos);
        if (entry->inode == 0 || entry->entry_size == 0)
            break;
        if (pos + entry->entry_size > n)
            break;
        memcpy(name, entry->name, entry->name_length);
        name[entry->name_length] = '\0';
        printf("%-10u %-6s %s\n", entry->inode, get_dir_type(entry->type), name);
        pos += entry->entry_size;
    }
    return 0;
}
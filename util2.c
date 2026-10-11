#include "ext2.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

struct callback_ctx
{
    fs_info *fs;
    uint32_t *rem;
};

int32_t callback(uint32_t block_num, void *data)
{
    uint8_t buf[EXT2_MAX_BLOCK_SIZE];
    uint32_t n;
    struct callback_ctx *ctx = data;

    if (*ctx->rem == 0)
        return 0;

    n = (*ctx->rem < ctx->fs->block_size) ? *ctx->rem : ctx->fs->block_size;
    if (block_num == 0)
        memset(buf, 0, n);

    else if (read_block(ctx->fs, block_num, buf) < 0)
        return -1;

    fwrite(buf, 1, n, stdout);
    *ctx->rem -= n;
    return 0;
}

int main(int argc, char *argv[])
{
    fs_info fs;
    int num;
    inode ind;
    uint32_t remaining;

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

    remaining = fix_end_32(ind.size);
    struct callback_ctx ctx = {&fs, &remaining};
    read_data(&fs, &ind, callback, &ctx);
    close(fs.fd);
    return 0;
}
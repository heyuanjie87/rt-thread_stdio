#ifndef __ROMFS2_H__
#define __ROMFS2_H__

#include <stdint.h>

#define ROMFS2_MAGIC    0x32534652

#define ROMFS2_DIRENT_FILE    1
#define ROMFS2_DIRENT_DIR      2

struct romfs2_dirent
{
    uint32_t type;
    uint32_t size;
    uint8_t *data;    /* ralative address of data */
    uint8_t * pos;    /* relative address of dirent */
    char name[16];
 };

struct romfs2_head
{
    uint32_t magic;
    uint32_t volume;

    struct romfs2_dirent root;
};

#endif

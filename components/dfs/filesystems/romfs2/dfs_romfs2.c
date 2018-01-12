/*
 * File      : dfs_romfs2.c
 * This file is part of Device File System in RT-Thread RTOS
 * COPYRIGHT (C) 2004-2011, RT-Thread Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Change Logs:
 * Date           Author       Notes
 * 20180112       heyuanjie87  used to execute binary program on flash
 */

#include <rtthread.h>
#include <dfs.h>
#include <dfs_fs.h>
#include <dfs_file.h>
#include <dirent.h>
#include <sys/stat.h>
#include "romfs2.h"

static struct romfs2_dirent *dfs_romfs2_lookup(struct romfs2_dirent *root, const char *path, size_t *size);

static int dfs_romfs2_mount(struct dfs_filesystem *fs, unsigned long rwflag, const void *data)
{
    struct romfs2_head *hdr;

    if (data == NULL)
        return -EIO;

    hdr = (struct romfs2_head *)data;
    if (hdr->magic != ROMFS2_MAGIC)
        return -ENOTBLK;

    fs->data = hdr;

    return 0;
}

static int dfs_romfs2_unmount(struct dfs_filesystem *fs)
{
    return 0;
}

static int dfs_romfs2_stat(struct dfs_filesystem *fs, const char *path, struct stat *st)
{
    size_t size;
    struct romfs2_dirent *dirent;
    struct romfs2_dirent *root_dirent;

    root_dirent = &(((struct romfs2_head *)fs->data)->root);
    dirent = dfs_romfs2_lookup(root_dirent, path, &size);

    if (dirent == NULL)
        return -ENOENT;

    st->st_dev = 0;
    st->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH |
    S_IWUSR | S_IWGRP | S_IWOTH;

    if (dirent->type == ROMFS2_DIRENT_DIR)
    {
        st->st_mode &= ~S_IFREG;
        st->st_mode |= S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
    }

    st->st_size = dirent->size;
    st->st_mtime = 0;

    return 0;
}

rt_inline int check_dirent(struct romfs2_dirent *dirent)
{
    if ((dirent->type != ROMFS2_DIRENT_FILE && dirent->type != ROMFS2_DIRENT_DIR)
        || dirent->size == 0)
        return -1;

    return 0;
}

rt_inline uint8_t* relocate_data(struct romfs2_dirent *d)
{
    return (uint8_t*)d - d->pos + d->data;
}

static struct romfs2_dirent *dfs_romfs2_lookup(struct romfs2_dirent *root, const char *path, size_t *size)
{
    size_t index, found;
    const char *subpath, *subpath_end;
    struct romfs2_dirent *dirent;
    size_t dirent_size;

    /* Check the root_dirent. */
    if (check_dirent(root) != 0)
        return NULL;

    if (path[0] == '/' && path[1] == '\0')
    {
        *size = root->size;
        return root;
    }

    /* goto root directy entries */
    dirent = (struct romfs2_dirent *)relocate_data(root);
    dirent_size = root->size;

    /* get the end position of this subpath */
    subpath_end = path;
    /* skip /// */
    while (*subpath_end && *subpath_end == '/')
        subpath_end ++;
    subpath = subpath_end;
    while ((*subpath_end != '/') && *subpath_end)
        subpath_end ++;

    while (dirent != NULL)
    {
        found = 0;

        /* search in folder */
        for (index = 0; index < dirent_size; index ++)
        {
            if (check_dirent(&dirent[index]) != 0)
                return NULL;
            if (rt_strlen(dirent[index].name) == (subpath_end - subpath) &&
                    rt_strncmp(dirent[index].name, subpath, (subpath_end - subpath)) == 0)
            {
                dirent_size = dirent[index].size;

                /* skip /// */
                while (*subpath_end && *subpath_end == '/')
                    subpath_end ++;
                subpath = subpath_end;
                while ((*subpath_end != '/') && *subpath_end)
                    subpath_end ++;

                if (!(*subpath))
                {
                    *size = dirent_size;
                    return &dirent[index];
                }

                if (dirent[index].type == ROMFS2_DIRENT_DIR)
                {
                    /* enter directory */
                    dirent = (struct romfs2_dirent *)relocate_data(&dirent[index]);
                    found = 1;
                    break;
                }
                else
                {
                    /* return file dirent */
                    if (subpath != NULL)
                        break; /* not the end of path */

                    return &dirent[index];
                }
            }
        }

        if (!found)
            break; /* not found */
    }

    /* not found */
    return NULL;
}

static int dfs_romfs2_ioctl(struct dfs_fd *file, int cmd, void *args)
{
    int ret = -EIO;
    struct romfs2_dirent *d;

    switch (cmd)
    {
    case XIPGFDA:
    {
        d = (struct romfs2_dirent *)file->data;
        if ((args != RT_NULL) && (d->type == ROMFS2_DIRENT_FILE))
        {
            *((int*)args) = (int)relocate_data(d);
            ret = 0;
        }
    }break;
    default:
    break;
    }

    return ret;
}

static int dfs_romfs2_read(struct dfs_fd *file, void *buf, size_t count)
{
    size_t length;
    struct romfs2_dirent *dirent;

    dirent = (struct romfs2_dirent *)file->data;
    RT_ASSERT(dirent != NULL);

    if (check_dirent(dirent) != 0)
    {
        return -EIO;
    }

    if (count < file->size - file->pos)
        length = count;
    else
        length = file->size - file->pos;

    if (length > 0)
    {
        uint8_t *data;

        data = relocate_data(dirent);
        rt_memcpy(buf, &data[file->pos], length);
    }

    /* update file current position */
    file->pos += length;

    return length;
}

static int dfs_romfs2_lseek(struct dfs_fd *file, off_t offset)
{
    if (offset <= file->size)
    {
        file->pos = offset;
        return file->pos;
    }

    return -EIO;
}

static int dfs_romfs2_close(struct dfs_fd *file)
{
    file->data = NULL;
    return 0;
}

static int dfs_romfs2_open(struct dfs_fd *file)
{
    size_t size;
    struct romfs2_dirent *dirent;
    struct romfs2_head *hdr;
    struct dfs_filesystem *fs;

    fs = (struct dfs_filesystem *)file->data;
    hdr = (struct romfs2_head *)fs->data;

    dirent = &hdr->root;
    if (check_dirent(dirent) != 0)
        return -EIO;

    if (file->flags & (O_CREAT | O_WRONLY | O_APPEND | O_TRUNC | O_RDWR))
        return -EINVAL;

    dirent = dfs_romfs2_lookup(dirent, file->path, &size);
    if (dirent == NULL)
        return -ENOENT;

    /* entry is a directory file type */
    if (dirent->type == ROMFS2_DIRENT_DIR)
    {
        if (!(file->flags & O_DIRECTORY))
            return -ENOENT;
    }
    else
    {
        /* entry is a file, but open it as a directory */
        if (file->flags & O_DIRECTORY)
            return -ENOENT;
    }

    file->data = dirent;
    file->size = size;
    file->pos = 0;

    return 0;
}

static int dfs_romfs2_getdents(struct dfs_fd *file, struct dirent *dirp, uint32_t count)
{
    size_t index;
    const char *name;
    struct dirent *d;
    struct romfs2_dirent *rd, *subrd;

    rd = (struct romfs2_dirent *)file->data;

    if (rd->type != ROMFS2_DIRENT_DIR)
        return -EIO;

    /* enter directory */
    rd = (struct romfs2_dirent *)relocate_data(rd);

    /* make integer count */
    count = (count / sizeof(struct dirent));
    if (count == 0)
        return -EINVAL;

    index = 0;
    for (index = 0; index < count && file->pos < file->size; index ++)
    {
        d = dirp + index;
        subrd = (struct romfs2_dirent *)((uint32_t)rd + file->pos);
        name = subrd->name;

        /* fill dirent */
        if (subrd->type == ROMFS2_DIRENT_DIR)
            d->d_type = DT_DIR;
        else
            d->d_type = DT_REG;

        d->d_namlen = rt_strlen(name);
        d->d_reclen = (rt_uint16_t)sizeof(struct dirent);
        rt_strncpy(d->d_name, name, d->d_namlen + 1);

        /* move to next position */
        file->pos += sizeof(*subrd);
    }

    return index * sizeof(struct dirent);
}

static const struct dfs_file_ops _rom2_fops =
{
    dfs_romfs2_open,
    dfs_romfs2_close,
    dfs_romfs2_ioctl,
    dfs_romfs2_read,
    NULL,
    NULL,
    dfs_romfs2_lseek,
    dfs_romfs2_getdents,
};

static const struct dfs_filesystem_ops _romfs2 =
{
    "romfs2",
    DFS_FS_FLAG_DEFAULT,
    &_rom2_fops,

    dfs_romfs2_mount,
    dfs_romfs2_unmount,
    NULL,
    NULL,

    NULL,
    dfs_romfs2_stat,
    NULL,
};

int dfs_romfs2_init(void)
{
    /* register rom file system */
    dfs_register(&_romfs2);
    return 0;
}
INIT_COMPONENT_EXPORT(dfs_romfs2_init);

/*
 * File      : dfs.c
 * This file is part of Device File System in RT-Thread RTOS
 * COPYRIGHT (C) 2004-2012, RT-Thread Development Team
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
 * 2005-02-22     Bernard      The first version.
 */

#include <dfs.h>
#include <dfs_fs.h>
#include <dfs_file.h>
#include "dfs_private.h"
#include <string.h>

#ifndef RT_USING_DFS_DEVONLY
/* Global variables */
const struct dfs_filesystem_ops *filesystem_operation_table[DFS_FILESYSTEM_TYPES_MAX];
struct dfs_filesystem filesystem_table[DFS_FILESYSTEMS_MAX];

#ifdef DFS_USING_WORKDIR
char working_directory[DFS_PATH_MAX] = {"/"};
#endif
#endif

/* device filesystem lock */
static struct rt_mutex fslock;
static struct dfs_fdtable _fdtab;
static int fd_alloc(struct dfs_fdtable *fdt, int startfd);

/**
 * @addtogroup DFS
 */

/*@{*/

/**
 * this function will initialize device file system.
 */
int dfs_init(void)
{
    /* clean fd table */
    memset(&_fdtab, 0, sizeof(_fdtab));

    /* create device filesystem lock */
    rt_mutex_init(&fslock, "fslock", RT_IPC_FLAG_FIFO);

#ifndef RT_USING_DFS_DEVONLY
    /* clear filesystem operations table */
    memset((void *)filesystem_operation_table, 0, sizeof(filesystem_operation_table));
    /* clear filesystem table */
    memset(filesystem_table, 0, sizeof(filesystem_table));

#ifdef DFS_USING_WORKDIR
    /* set current working directory */
    memset(working_directory, 0, sizeof(working_directory));
    working_directory[0] = '/';
#endif
#endif
    return 0;
}
INIT_PREV_EXPORT(dfs_init);

/**
 * this function will lock device file system.
 *
 * @note please don't invoke it on ISR.
 */
void dfs_lock(void)
{
    rt_err_t result;

    result = rt_mutex_take(&fslock, RT_WAITING_FOREVER);
    if (result != RT_EOK)
    {
        RT_ASSERT(0);
    }
}

/**
 * this function will lock device file system.
 *
 * @note please don't invoke it on ISR.
 */
void dfs_unlock(void)
{
    rt_mutex_release(&fslock);
}

/**
 * @ingroup Fd
 * This function will allocate a file descriptor.
 *
 * @param minfd minimum file descriptor want to allocated
 *
 * @return -1 on failed or the allocated file descriptor.
 */
int fd_new(int minfd)
{
    struct dfs_fd *d;
    int idx;
    struct dfs_fdtable *fdt;

    fdt = dfs_fdtable_get();
    /* lock filesystem */
    dfs_lock();

    /* find an empty fd entry */
    idx = fd_alloc(fdt, minfd);

    /* can't find an empty fd entry */
    if (idx >= fdt->maxfd)
    {
        idx = -1;
        goto __result;
    }

    d = fdt->fds[idx];
    d->ref_count = 1;
    d->magic = DFS_FD_MAGIC;

__result:
    dfs_unlock();
    return idx;
}

/**
 * @ingroup Fd
 *
 * This function will return a file descriptor structure according to file
 * descriptor.
 *
 * @return NULL on on this file descriptor or the file descriptor structure
 * pointer.
 */
struct dfs_fd *fd_get(int fd)
{
    struct dfs_fd *d;
    struct dfs_fdtable *fdt;

    fdt = dfs_fdtable_get();
    if (fd < 0 || fd >= fdt->maxfd)
        return NULL;

    dfs_lock();
    d = fdt->fds[fd];

    /* check dfs_fd valid or not */
    if (d->magic != DFS_FD_MAGIC)
    {
        dfs_unlock();
        return NULL;
    }

    /* increase the reference count */
    d->ref_count ++;
    dfs_unlock();

    return d;
}

/**
 * @ingroup Fd
 *
 * This function will put the file descriptor.
 */
void fd_put(struct dfs_fd *fd)
{
    RT_ASSERT(fd != NULL);

    dfs_lock();
    fd->ref_count --;

    /* clear this fd entry */
    if (fd->ref_count == 0)
    {
        memset(fd, 0, sizeof(struct dfs_fd));
    }
    dfs_unlock();
};

#ifndef RT_USING_DFS_DEVONLY
/**
 * @ingroup Fd
 *
 * This function will return whether this file has been opend.
 *
 * @param pathname the file path name.
 *
 * @return 0 on file has been open successfully, -1 on open failed.
 */
int fd_is_open(const char *pathname)
{
    char *fullpath;
    unsigned int index;
    struct dfs_filesystem *fs;
    struct dfs_fd *fd;
    struct dfs_fdtable *fdt;

    fdt = dfs_fdtable_get();
    fullpath = dfs_normalize_path(NULL, pathname);
    if (fullpath != NULL)
    {
        char *mountpath;
        fs = dfs_filesystem_lookup(fullpath);
        if (fs == NULL)
        {
            /* can't find mounted file system */
            free(fullpath);

            return -1;
        }

        /* get file path name under mounted file system */
        if (fs->path[0] == '/' && fs->path[1] == '\0')
            mountpath = fullpath;
        else
            mountpath = fullpath + strlen(fs->path);

        dfs_lock();

        for (index = 0; index < fdt->maxfd; index++)
        {
            fd = fdt->fds[index];
            if (fd->fops == NULL)
                continue;

            if (fd->fops == fs->ops->fops && strcmp(fd->path, mountpath) == 0)
            {
                /* found file in file descriptor table */
                rt_free(fullpath);
                dfs_unlock();

                return 0;
            }
        }
        dfs_unlock();

        rt_free(fullpath);
    }

    return -1;
}

/**
 * this function will return a sub-path name under directory.
 *
 * @param directory the parent directory.
 * @param filename the filename.
 *
 * @return the subdir pointer in filename
 */
const char *dfs_subdir(const char *directory, const char *filename)
{
    const char *dir;

    if (strlen(directory) == strlen(filename)) /* it's a same path */
        return NULL;

    dir = filename + strlen(directory);
    if ((*dir != '/') && (dir != filename))
    {
        dir --;
    }

    return dir;
}
RTM_EXPORT(dfs_subdir);

/**
 * this function will normalize a path according to specified parent directory
 * and file name.
 *
 * @param directory the parent path
 * @param filename the file name
 *
 * @return the built full file path (absolute path)
 */
char *dfs_normalize_path(const char *directory, const char *filename)
{
    char *fullpath;
    char *dst0, *dst, *src;

    /* check parameters */
    RT_ASSERT(filename != NULL);

#ifdef DFS_USING_WORKDIR
    if (directory == NULL) /* shall use working directory */
        directory = &working_directory[0];
#else
    if ((directory == NULL) && (filename[0] != '/'))
    {
        rt_kprintf(NO_WORKING_DIR);

        return NULL;
    }
#endif

    if (filename[0] != '/') /* it's a absolute path, use it directly */
    {
        fullpath = rt_malloc(strlen(directory) + strlen(filename) + 2);

        if (fullpath == NULL)
            return NULL;

        /* join path and file name */
        rt_snprintf(fullpath, strlen(directory) + strlen(filename) + 2,
            "%s/%s", directory, filename);
    }
    else
    {
        fullpath = rt_strdup(filename); /* copy string */

        if (fullpath == NULL)
            return NULL;
    }

    src = fullpath;
    dst = fullpath;

    dst0 = dst;
    while (1)
    {
        char c = *src;

        if (c == '.')
        {
            if (!src[1]) src ++; /* '.' and ends */
            else if (src[1] == '/')
            {
                /* './' case */
                src += 2;

                while ((*src == '/') && (*src != '\0'))
                    src ++;
                continue;
            }
            else if (src[1] == '.')
            {
                if (!src[2])
                {
                    /* '..' and ends case */
                    src += 2;
                    goto up_one;
                }
                else if (src[2] == '/')
                {
                    /* '../' case */
                    src += 3;

                    while ((*src == '/') && (*src != '\0'))
                        src ++;
                    goto up_one;
                }
            }
        }

        /* copy up the next '/' and erase all '/' */
        while ((c = *src++) != '\0' && c != '/')
            *dst ++ = c;

        if (c == '/')
        {
            *dst ++ = '/';
            while (c == '/')
                c = *src++;

            src --;
        }
        else if (!c)
            break;

        continue;

up_one:
        dst --;
        if (dst < dst0)
        {
            rt_free(fullpath);
            return NULL;
        }
        while (dst0 < dst && dst[-1] != '/')
            dst --;
    }

    *dst = '\0';

    /* remove '/' in the end of path if exist */
    dst --;
    if ((dst != fullpath) && (*dst == '/'))
        *dst = '\0';

    return fullpath;
}
RTM_EXPORT(dfs_normalize_path);
#endif

/**
 * This function will get the file descriptor table of current process.
 */
struct dfs_fdtable* dfs_fdtable_get(void)
{
    struct dfs_fdtable *fdt;

    fdt = &_fdtab;

    return fdt;
}

static int fd_alloc(struct dfs_fdtable *fdt, int startfd)
{
    int idx;

    /* find an empty fd entry */
    for (idx = startfd; idx < fdt->maxfd; idx++)
    {
        if (fdt->fds[idx] == RT_NULL)
            break;
        if (fdt->fds[idx]->ref_count == 0)
            break;
    }
    /* allocate a larger FD container */
    if (idx == fdt->maxfd && fdt->maxfd < DFS_FD_MAX)
    {
        int cnt;
        struct dfs_fd **fds;

        cnt = fdt->maxfd + 4;
        cnt = cnt > DFS_FD_MAX? DFS_FD_MAX : cnt;

        fds = rt_malloc(cnt * sizeof(struct dfs_fd *));
        if (fds == RT_NULL)
            goto __out;

        rt_memset(fds, 0, cnt * sizeof(struct dfs_fd *));
        rt_memcpy(fds, fdt->fds, fdt->maxfd*sizeof(struct dfs_fd *));
        rt_free(fdt->fds);
        fdt->fds = fds;
        fdt->maxfd = cnt;
    }
    /* allocate  'struct dfs_fd' */
    if (idx < fdt->maxfd &&fdt->fds[idx] == RT_NULL)
    {
        fdt->fds[idx] = rt_malloc(sizeof(struct dfs_fd));
        if (fdt->fds[idx] == RT_NULL)
            idx = fdt->maxfd;
    }

__out:
    return idx;
}

/**
 * @ingroup Fd
 *
 * This function will free unused file descriptor.
 */
void fd_free(int fd)
{
    struct dfs_fdtable *fdt;

    fdt = dfs_fdtable_get();

    dfs_lock();
    if (fdt->fds[fd]->ref_count > 0)
        goto _out;
    rt_free(fdt->fds[fd]);
    fdt->fds[fd] = RT_NULL;
_out:
    dfs_unlock();
}
/*@}*/

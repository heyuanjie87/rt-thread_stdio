/*
 * File      : dfs.h
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

#ifndef __DFS_H__
#define __DFS_H__

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#include <time.h>
#include <rtthread.h>
#include <rtdevice.h>

#ifndef DFS_FILESYSTEMS_MAX
#define DFS_FILESYSTEMS_MAX     2
#endif

#ifndef DFS_FD_MAX
#define DFS_FD_MAX              4
#endif

#ifndef DFS_PATH_MAX
#define DFS_PATH_MAX             256
#endif

#ifndef SECTOR_SIZE
#define SECTOR_SIZE              512
#endif

#ifndef DFS_FILESYSTEM_TYPES_MAX
#define DFS_FILESYSTEM_TYPES_MAX 2
#endif

#define DFS_FS_FLAG_DEFAULT     0x00    /* default flag */
#define DFS_FS_FLAG_FULLPATH    0x01    /* set full path to underlaying file system */

#define DFS_DEBUG_INFO          0x01
#define DFS_DEBUG_WARNING       0x02
#define DFS_DEBUG_ERROR         0x04
#define DFS_DEBUG_LEVEL         (DFS_DEBUG_INFO | DFS_DEBUG_WARNING | DFS_DEBUG_ERROR)

#define dfs_log(...)

/* File types */
#define FT_REGULAR               0   /* regular file */
#define FT_SOCKET                1   /* socket file  */
#define FT_DIRECTORY             2   /* directory    */
#define FT_USER                  3   /* user defined */

/* File flags */
#define DFS_F_OPEN              0x01000000
#define DFS_F_DIRECTORY         0x02000000
#define DFS_F_EOF               0x04000000
#define DFS_F_ERR               0x08000000

#ifdef __cplusplus
extern "C" {
#endif

/* Initialization of dfs */
int dfs_init(void);

char *dfs_normalize_path(const char *directory, const char *filename);
const char *dfs_subdir(const char *directory, const char *filename);

void dfs_lock(void);
void dfs_unlock(void);

/* FD APIs */
int fd_new(int minfd);
struct dfs_fd *fd_get(int fd);
void fd_put(struct dfs_fd *fd);
int fd_is_open(const char *pathname);

#ifdef __cplusplus
}
#endif

#endif

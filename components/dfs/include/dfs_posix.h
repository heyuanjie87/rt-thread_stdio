/*
 * File      : dfs_posix.h
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
 * 2009-05-27     Yi.qiu       The first version.
 * 2010-07-18     Bernard      add stat and statfs structure definitions.
 * 2011-05-16     Yi.qiu       Change parameter name of rename, "new" is C++ key word.
 */

#ifndef __DFS_POSIX_H__
#define __DFS_POSIX_H__

#include <sys/types.h>
#include <dirent.h>

struct stat;
struct statfs;

/* file api*/
int open(const char *file, int flags, int mode);
int close(int d);
#ifdef RT_USING_NEWLIB
_READ_WRITE_RETURN_TYPE _EXFUN(read, (int __fd, void *__buf, size_t __nbyte));
_READ_WRITE_RETURN_TYPE _EXFUN(write, (int __fd, const void *__buf, size_t __nbyte));
#else
int read(int fd, void *buf, size_t len);
int write(int fd, const void *buf, size_t len);
#endif
off_t lseek(int fd, off_t offset, int whence);
int rename(const char *from, const char *to);
int unlink(const char *pathname);
int stat(const char *file, struct stat *buf);
int fstat(int fildes, struct stat *buf);
int fsync(int fildes);
int ioctl(int fildes, unsigned long cmd, void *data);
int fcntl(int fd, unsigned int cmd, unsigned long arg);

/* directory api*/
int rmdir(const char *path);
int chdir(const char *path);
char *getcwd(char *buf, size_t size);

/* file system api */
int statfs(const char *path, struct statfs *buf);

int access(const char *path, int amode);

#ifdef __cplusplus
}
#endif

#endif

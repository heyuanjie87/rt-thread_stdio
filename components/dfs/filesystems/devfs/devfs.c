/*
 * File      : devfs.c
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
 */

#include <rtthread.h>
#include <rtdevice.h>

#include <dfs.h>
#include <dfs_fs.h>
#include <dfs_file.h>
#include <dirent.h>
#include <sys/stat.h>

#include "devfs.h"

#ifndef RT_USING_DFS_DEVONLY
struct device_dirent
{
    rt_device_t *devices;
    rt_uint16_t read_index;
    rt_uint16_t device_count;
};

int dfs_device_fs_mount(struct dfs_filesystem *fs, unsigned long rwflag, const void *data)
{
    return 0;
}

static int dfs_device_fs_open(struct dfs_fd *file)
{
    rt_err_t result;
    rt_device_t device;

    device = rt_device_find(&file->path[1]);
    if (device == RT_NULL)
        return -ENODEV;

	if (device->fops)
	{
		/* use device fops */
		file->fops = device->fops;
		file->dev = device;

		/* use fops */
		if (file->fops->open)
		{
			result = file->fops->open(file);

				return result;
		}
	}
	else
	{
			return -ENOSYS;
	}

	file->dev = RT_NULL;

    /* open device failed. */
    return -EIO;
}

static const struct dfs_file_ops _device_fops =
{
    dfs_device_fs_open,
    RT_NULL,
    RT_NULL,
    RT_NULL,
    RT_NULL,
    RT_NULL,					/* flush */
    RT_NULL,					/* lseek */
    RT_NULL,
	RT_NULL,
};

static const struct dfs_filesystem_ops _device_fs =
{
    "devfs",
    DFS_FS_FLAG_DEFAULT,
    &_device_fops,

    dfs_device_fs_mount,
    RT_NULL,
    RT_NULL,
    RT_NULL,

    RT_NULL,
    RT_NULL,
    RT_NULL,
};

int devfs_init(void)
{
    /* register rom file system */
    dfs_register(&_device_fs);

    return 0;
}
INIT_FS_EXPORT(devfs_init);
#endif

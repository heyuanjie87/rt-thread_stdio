/*
 * File      : poll.h
 * This file is part of Device File System in RT-Thread RTOS
 * COPYRIGHT (C) 2006-2016, RT-Thread Development Team
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
 * 2016-09-19     Heyuanjie    The first version.
 * 2016-12-26     Bernard      Update poll interface
 */
#ifndef __POLL_H__
#define __POLL_H__

#include <rtthread.h>
#include <rtdevice.h>

#ifdef __cplusplus
extern "C" {
#endif

#define POLLIN          (0x01)
#define POLLRDNORM      (0x01)
#define POLLRDBAND      (0x01)
#define POLLPRI         (0x01)

#define POLLOUT         (0x02)
#define POLLWRNORM      (0x02)
#define POLLWRBAND      (0x02)

#define POLLERR         (0x04)
#define POLLHUP         (0x08)
#define POLLNVAL        (0x10)

#define POLLMASK_DEFAULT (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM)

typedef unsigned int nfds_t;

struct pollfd
{
    int fd;
    short events;
    short revents;
};

int poll(struct pollfd *fds, nfds_t nfds, int timeout);

#ifdef __cplusplus
}
#endif

#endif

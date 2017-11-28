#ifndef RTC_H_INCLUDED
#define RTC_H_INCLUDED

/*
 * File      : rtc.h
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006 - 2012, RT-Thread Development Team
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
 * 2012-10-10     aozima       first version.
 */

#include <rtthread.h>

struct rtc_time 
{
	char tm_sec;
	char tm_min;
	char tm_hour;
	char tm_mday;
	char tm_mon;
	short tm_year;
};

typedef struct rt_rtc
{
    struct rt_device parent;
	
	const struct rt_rtc_ops *ops;
}rt_rtc_t;

struct rt_rtc_ops
{
    int (*get_time)(rt_rtc_t *rtc, struct rtc_time *tm);
	int (*set_time)(rt_rtc_t *rtc, struct rtc_time *tm);
	int (*control)(rt_rtc_t *rtc, int cmd, void *args);
};

rt_err_t rt_hw_rtc_register(rt_rtc_t *rtc, const char *name, void *data);

#endif // RTC_H_INCLUDED

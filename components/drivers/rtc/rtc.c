/*
 * File      : rtc.c
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
 * 2012-01-29     aozima       first version.
 * 2012-04-12     aozima       optimization: find rtc device only first.
 * 2012-04-16     aozima       add scheduler lock for set_date and set_time.
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <dfs_file.h>

static const unsigned char rtc_days_in_month[] = 
{
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

rt_inline int is_leap_year(unsigned int year)
{
	return (!(year % 4) && (year % 100)) || !(year % 400);
}

int rtc_month_days(unsigned int month, unsigned int year)
{
	return rtc_days_in_month[month] + (is_leap_year(year) && month == 1);
}

static int rtc_open(struct dfs_fd *fd)
{

    return 0;
}

static int rtc_close(struct dfs_fd *fd)
{
	
    return 0;
}

static int rtc_valid_tm(struct rtc_time *tm)
{
	if (tm->tm_year < 70
		|| ((rt_uint8_t)tm->tm_mon) >= 12
		|| tm->tm_mday < 1
		|| tm->tm_mday > rtc_month_days(tm->tm_mon, tm->tm_year + 1900)
		|| ((rt_uint8_t)tm->tm_hour) >= 24
		|| ((rt_uint8_t)tm->tm_min) >= 60
		|| ((rt_uint8_t)tm->tm_sec) >= 60)
		return -EINVAL;

	return 0;
}

static int rtc_control(struct dfs_fd *fd, int cmd, void *args)
{
	rt_rtc_t *rtc;
	int ret = 0;

	rtc = (rt_rtc_t *)fd->dev;
	
	switch (cmd)
	{
	case RTC_TIME_GET:
    {
		rtc->ops->get_time(rtc, (struct rtc_time*)args);
	}break;
    case RTC_TIME_SET:
    {
        struct rtc_time *tm;
        
		tm = (struct rtc_time *)args;
		ret = rtc_valid_tm(tm);
		if (ret != 0)
			break;

		ret = rtc->ops->set_time(rtc, tm);
	}break;
    default:
    {
	    ret = rtc->ops->control(rtc, cmd, args);
	}break;		
	}

    return ret;
}

static int rtc_read(struct dfs_fd *fd, void *buf, size_t count)
{
    return 0;
}

static int rtc_write(struct dfs_fd *fd, const void *buf, size_t count)
{

    return 0;
}

static int rtc_poll(struct dfs_fd *fd, rt_pollreq_t *req)
{
    int mask = 0;

	return mask;
}

static const struct dfs_file_ops rtc_fops = 
{
    rtc_open,
    rtc_close,
    rtc_control,	
    rtc_read,
    rtc_write,
	RT_NULL,
	RT_NULL,
	RT_NULL,
	rtc_poll,
};

rt_err_t rt_hw_rtc_register(rt_rtc_t *rtc, const char *name, void *data)
{
	rt_device_t device;

	device = &rtc->parent;

	device->type = RT_Device_Class_RTC;
	device->fops = &rtc_fops;

	device->user_data = data;

	return rt_device_register(device, name, 0);
}

/*
 * File      : device.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006 - 2013, RT-Thread Development Team
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
 * 2007-01-21     Bernard      the first version
 * 2010-05-04     Bernard      add rt_device_init implementation
 * 2012-10-20     Bernard      add device check in register function,
 *                             provided by Rob <rdent@iinet.net.au>
 * 2012-12-25     Bernard      return RT_EOK if the device interface not exist.
 * 2013-07-09     Grissiom     add ref_count support
 * 2016-04-02     Bernard      fix the open_flag initialization issue.
 */

#include <rtthread.h>

/**
 * This function registers a device driver with specified name.
 *
 * @param dev the pointer of device driver structure
 * @param name the device driver's name
 * @param flags the flag of device
 *
 * @return the error code, RT_EOK on initialization successfully.
 */
rt_err_t rt_device_register(rt_device_t dev,
                            const char *name,
                            rt_uint16_t flags)
{
    if (dev == RT_NULL)
        return -RT_ERROR;

    if (rt_device_find(name) != RT_NULL)
        return -RT_ERROR;

    rt_object_init(&(dev->parent), RT_Object_Class_Device, name);
    dev->flag = flags;
    dev->ref_count = 0;
    dev->open_flag = 0;

#if defined(RT_USING_POSIX)
    dev->fops = RT_NULL;
#endif

    return RT_EOK;
}
RTM_EXPORT(rt_device_register);

/**
 * This function removes a previously registered device driver
 *
 * @param dev the pointer of device driver structure
 *
 * @return the error code, RT_EOK on successfully.
 */
rt_err_t rt_device_unregister(rt_device_t dev)
{
    RT_ASSERT(dev != RT_NULL);

    rt_object_detach(&(dev->parent));

    return RT_EOK;
}
RTM_EXPORT(rt_device_unregister);

/**
 * This function finds a device driver by specified name.
 *
 * @param name the device driver's name
 *
 * @return the registered device driver on successful, or RT_NULL on failure.
 */
rt_device_t rt_device_find(const char *name)
{
    struct rt_object *object;
    struct rt_list_node *node;
    struct rt_object_information *information;

    extern struct rt_object_information rt_object_container[];

    /* enter critical */
    if (rt_thread_self() != RT_NULL)
        rt_enter_critical();

    /* try to find device object */
    information = &rt_object_container[RT_Object_Class_Device];
    for (node  = information->object_list.next;
         node != &(information->object_list);
         node  = node->next)
    {
        object = rt_list_entry(node, struct rt_object, list);
        if (rt_strncmp(object->name, name, RT_NAME_MAX) == 0)
        {
            /* leave critical */
            if (rt_thread_self() != RT_NULL)
                rt_exit_critical();

            return (rt_device_t)object;
        }
    }

    /* leave critical */
    if (rt_thread_self() != RT_NULL)
        rt_exit_critical();

    /* not found */
    return RT_NULL;
}
RTM_EXPORT(rt_device_find);

/**
 * This function will initialize the specified device
 *
 * @param dev the pointer of device driver structure
 *
 * @return the result
 */
rt_err_t rt_device_init(rt_device_t dev)
{
    rt_err_t result = RT_EOK;

    RT_ASSERT(dev != RT_NULL);

    /* get device init handler */
    if (dev->dops->init != RT_NULL)
    {
        if (!(dev->flag & RT_DEVICE_FLAG_ACTIVATED))
        {
            result = dev->dops->init(dev);
            if (result != RT_EOK)
            {
                rt_kprintf("To initialize device:%s failed. The error code is %d\n",
                           dev->parent.name, result);
            }
            else
            {
                dev->flag |= RT_DEVICE_FLAG_ACTIVATED;
            }
        }
    }

    return result;
}

/**
 * This function will perform a variety of control functions on devices.
 *
 * @param dev the pointer of device driver structure
 * @param cmd the command sent to device
 * @param arg the argument of command
 *
 * @return the result
 */
rt_err_t rt_device_control(rt_device_t dev, int cmd, void *arg)
{
    RT_ASSERT(dev != RT_NULL);

    /* call device write interface */
    if (dev->dops->control != RT_NULL)
    {
        return dev->dops->control(dev, cmd, arg);
    }

    return -RT_ENOSYS;
}

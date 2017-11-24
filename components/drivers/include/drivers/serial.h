/*
 * File      : serial.h
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
 * 2012-05-15     lgnq         first version.
 * 2012-05-28     bernard      change interfaces
 * 2013-02-20     bernard      use RT_SERIAL_RB_BUFSZ to define
 *                             the size of ring buffer.
 */

#ifndef __SERIAL_H__
#define __SERIAL_H__

#include <rtdevice.h>
#include <termios.h>

#define RT_SERIAL_EVENT_RX_IND          0x01    /* Rx indication */
#define RT_SERIAL_EVENT_TX_DONE         0x02    /* Tx complete   */
#define RT_SERIAL_EVENT_RX_DMADONE      0x03    /* Rx DMA transfer done */
#define RT_SERIAL_EVENT_TX_DMADONE      0x04    /* Tx DMA transfer done */
#define RT_SERIAL_EVENT_RX_TIMEOUT      0x05    /* Rx timeout    */

#define SERIAL_CTRL_TXSTART    0x010000
#define SERIAL_CTRL_TXSTOP     0x020000
#define SERIAL_CTRL_RXSTART    0x030000
#define SERIAL_CTRL_RXSTOP     0x040000

struct rt_serial_device
{
    struct rt_device parent;

    const struct rt_uart_ops *ops;

    rt_ringbuffer_t *rxfifo;
    rt_ringbuffer_t *txfifo;

    struct termios termcfg;
    rt_uint8_t tx_started;

    rt_wqueue_t reader_queue;
    rt_wqueue_t writer_queue;
};
typedef struct rt_serial_device rt_serial_t;

/**
 * uart operators
 */
struct rt_uart_ops
{
    int (*init)(rt_serial_t *serial);
    void (*deinit)(rt_serial_t *serial);
    int (*control)(rt_serial_t *serial, int cmd, void *arg);
    int (*set_termios)(rt_serial_t *serial, struct termios *termcfg);
    int (*putc)(rt_serial_t *serial, char c);
    int (*getc)(rt_serial_t *serial);
};

void rt_hw_serial_isr(rt_serial_t *serial, int event);

rt_err_t rt_hw_serial_register(rt_serial_t *serial,
                               const char  *name,
                               rt_uint32_t flag,
                               void        *data);

#endif

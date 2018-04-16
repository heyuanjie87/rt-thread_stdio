/*
 * File      : serial.c
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
 * 2006-03-13     bernard      first version
 * 2012-05-15     lgnq         modified according bernard's implementation.
 * 2012-05-28     bernard      code cleanup
 * 2012-11-23     bernard      fix compiler warning.
 * 2013-02-20     bernard      use RT_SERIAL_RB_BUFSZ to define
 *                             the size of ring buffer.
 * 2014-07-10     bernard      rewrite serial framework
 * 2014-12-31     bernard      use open_flag for poll_tx stream mode.
 * 2015-05-19     Quintin      fix DMA tx mod tx_dma->activated flag !=RT_FALSE BUG
 *                             in open function.
 * 2015-11-10     bernard      fix the poll rx issue when there is no data.
 * 2016-05-10     armink       add fifo mode to DMA rx when serial->config.bufsz != 0.
 */

#include <rthw.h>
#include <rtdevice.h>
#include <dfs_file.h>
#include <poll.h>

#ifndef RT_TERMIOS_SPEED
#define TERMIOS_SPEED_DEFAULT 115200
#else
#define TERMIOS_SPEED_DEFAULT RT_TERMIOS_SPEED
#endif

#define SERIAL_FIFO_SIZE    32

#define INIT_C_CC "\003\034\177\025\004\0\1\021\023"

static const struct termios tty_std_termios =
{
    ICRNL | IXON,
    OPOST | ONLCR,
    CS8 | CREAD | HUPCL,
    ISIG | ICANON | ECHO | ECHOE | ECHOK | IEXTEN,
    INIT_C_CC,
    TERMIOS_SPEED_DEFAULT,
};

static void termios_std_init(struct termios *term)
{
    rt_memset(term, 0, sizeof(struct termios));
    *term = tty_std_termios;
}

/*
 * This function initializes serial device.
 */
static int rt_serial_init(rt_serial_t *serial)
{
    int ret = 0;

    /* initialize rx/tx */
    serial->rxfifo = rt_ringbuffer_create(SERIAL_FIFO_SIZE);
    serial->txfifo = rt_ringbuffer_create(SERIAL_FIFO_SIZE);

	if (serial->rxfifo == RT_NULL || serial->txfifo == RT_NULL)
	{
	    rt_ringbuffer_destroy(serial->rxfifo);
		rt_ringbuffer_destroy(serial->txfifo);

		return -ENOMEM;
	}

    termios_std_init(&serial->termcfg);

    ret = serial->ops->init(serial);

    /* apply configuration */
    if (serial->ops->set_termios)
        ret = serial->ops->set_termios(serial, &serial->termcfg);

	if (ret == 0)
		 ret =  serial->ops->control(serial, SERIAL_CTRL_RXSTART, RT_NULL);

    return ret;
}

static int rt_serial_open(struct dfs_fd *fd)
{
    rt_serial_t *serial;
    int ret = 0;

    serial = (rt_serial_t *)fd->dev;
    serial->parent.ref_count ++;
	if (serial->parent.ref_count == 1)
	{
	    ret = rt_serial_init(serial);
	}

	return ret;
}

static int rt_serial_close(struct dfs_fd *fd)
{
    rt_serial_t *serial;

    serial = (rt_serial_t *)fd->dev;
    if (serial->parent.ref_count > 0)
    {
        serial->parent.ref_count --;
	    if (serial->parent.ref_count == 0)
	    {
            serial->ops->deinit(serial);
            rt_ringbuffer_destroy(serial->rxfifo);
            rt_ringbuffer_destroy(serial->txfifo);
            serial->rxfifo = RT_NULL;
            serial->txfifo = RT_NULL;
        }
    }

    return 0;
}

static int rt_serial_read(struct dfs_fd *fd, void *buffer, size_t size)
{
    rt_serial_t *serial;
    int rxlen = 0;
    uint8_t *dbuf;

    if (size == 0)
        return 0;

    serial = (rt_serial_t *)fd->dev;
    dbuf = buffer;

    do
    {
        rxlen += rt_ringbuffer_get(serial->rxfifo, &dbuf[rxlen], size - rxlen);

        if (rxlen > 0)
        {
            break;
        }
        else if (fd->flags & O_NONBLOCK)
        {
            rxlen = -EAGAIN;
            break;
        }

        rt_wqueue_wait(&serial->reader_queue, 0, -1);
    }
    while (rxlen < size);

    return rxlen;
}

#define O_OPOST(tty)	((tty)->termcfg.c_oflag & OPOST)
#define O_ONLCR(tty)	((tty)->termcfg.c_oflag & ONLCR)

static int do_output_char(unsigned char c, rt_serial_t *serial, int space)
{
    int	spaces;

    if (!space)
        return -1;

    switch (c)
    {
    case '\n':
        if (O_ONLCR(serial))
        {
            if (space < 2)
                return -1;

            rt_ringbuffer_put(serial->txfifo, "\r\n", 2);
            return 2;
        }
        break;
    default:
        break;
    }

    return 1;
}

static int process_output(unsigned char c, rt_serial_t *serial)
{
    int	space, retval;

    space = rt_ringbuffer_space_len(serial->txfifo);
    retval = do_output_char(c, serial, space);

    if (retval < 0)
        return -1;
    else
        return 0;
}

static int process_output_block(rt_serial_t *serial, const unsigned char *buf, unsigned int nr)
{
    int	space;
    int	i;
    const unsigned char *cp;

    space = rt_ringbuffer_space_len(serial->txfifo);
    if (!space)
    {
        return 0;
    }

    if (nr > space)
        nr = space;

    for (i = 0, cp = buf; i < nr; i++, cp++)
    {
        unsigned char c = *cp;

        switch (c)
        {
        case '\n':
            if (O_ONLCR(serial))
                goto break_out;
            break;
        case '\r':
            break;
        case '\t':
            goto break_out;
        case '\b':
            break;
        default:
            break;
        }
    }

break_out:
    i = rt_ringbuffer_put(serial->txfifo, buf, i);

    return i;
}

rt_inline void flush_char(rt_serial_t *serial)
{
    if (!serial->tx_started)
    {
        serial->tx_started = 1;

        serial->ops->control(serial, SERIAL_CTRL_TXSTART, 0);
    }
}

static int rt_serial_write(struct dfs_fd *file, const void *buf, size_t size)
{
    rt_serial_t *serial;
    uint8_t *dbuf;
    int retval;
    int c;

    if (size == 0)
        return 0;

    serial = (rt_serial_t *)file->dev;
    dbuf = (uint8_t*)buf;

    while (1)
    {
        if (O_OPOST(serial))
        {
            while (size > 0)
            {
                int num;

                num = process_output_block(serial, dbuf, size);
                if (num < 0)
                {
                    if (num == -EAGAIN)
                        break;
                    retval = num;
                    goto break_out;
                }
                dbuf += num;
                size -= num;
                if (size == 0)
                    break;
                c = *dbuf;
                if (process_output(c, serial) < 0)
                    break;
                dbuf++;
                size--;
            }

            flush_char(serial);
        }
        else
        {
            while (size > 0)
            {
                c = rt_ringbuffer_put(serial->txfifo, dbuf, size);

                if (c < 0)
                {
                    retval = c;
                    goto break_out;
                }
                if (!c)
                    break;
                dbuf += c;
                size -= c;

                flush_char(serial);
            }
        }

        if (!size)
            break;
        if (file->flags & O_NONBLOCK)
        {
            retval = -EAGAIN;
            break;
        }

        rt_wqueue_wait(&serial->writer_queue, 0, -1);
    }

break_out:

    return (dbuf - (uint8_t*)buf) ? dbuf - (uint8_t*)buf : retval;
}

static int rt_serial_control(struct dfs_fd *fd, int cmd, void *args)
{
    rt_serial_t *serial;
    struct termios *tc;
    int ret = 0;

    serial = (rt_serial_t *)fd->dev;

    switch (cmd & 0xFFFF)
    {
	case FIONREAD:
		*((int*)args) = rt_ringbuffer_data_len(serial->rxfifo);
		break;
    case TCSETS:
        tc = (struct termios*)args;

        serial->termcfg.c_oflag = tc->c_oflag;
        ret = serial->ops->set_termios(serial, tc);
        break;
    case TCGETS:
        tc = (struct termios*)args;

        *tc = serial->termcfg;
        break;
    case TCFLSH:
	{
	    int opt;

		opt = (int)args;
		switch (opt)
		{
		case TCIFLUSH:
			rt_ringbuffer_reset(serial->rxfifo);
		    break;
		case TCIOFLUSH:
			rt_ringbuffer_reset(serial->rxfifo);
		    rt_ringbuffer_reset(serial->txfifo);
			break;
		case TCOFLUSH:
			rt_ringbuffer_reset(serial->txfifo);
			break;
		}
	}break;

	case TOFIFOSETS:
	{
	    rt_ringbuffer_t *fifo, *f;
		int size;

		size = (int)args;
		if (size < 2)
			return -EINVAL;

		if (size == serial->txfifo->buffer_size)
			return 0;

		fifo = rt_ringbuffer_create(size);
		if (fifo == RT_NULL)
		{
			ret = -ENOMEM;
		    break;
		}

		f =  serial->txfifo;
		serial->txfifo = fifo;
		rt_ringbuffer_destroy(f);
	}break;
	case TIFIFOSETS:
	{
	    rt_ringbuffer_t *fifo, *f;
		int size;

		size = (int)args;
		if (size < 1)
			return -EINVAL;

		if (size == serial->rxfifo->buffer_size)
			return 0;

		fifo = rt_ringbuffer_create(size);
		if (fifo == RT_NULL)
		{
			ret = -ENOMEM;
		    break;
		}

		f =  serial->rxfifo;
		serial->rxfifo = fifo;
		rt_ringbuffer_destroy(f);
	}break;

    default :
        /* control device */
        ret = serial->ops->control(serial, cmd, args);
        break;
    }

    return ret;
}

static int rt_serial_poll(struct dfs_fd *fd, rt_pollreq_t *req)
{
    int mask = 0;
    rt_serial_t *serial;

    serial = (rt_serial_t *)fd->dev;

    rt_poll_add(&serial->writer_queue, req);
    rt_poll_add(&serial->reader_queue, req);

    if (rt_ringbuffer_data_len(serial->rxfifo) != 0)
    {
        mask |= POLLIN;
    }

    if (rt_ringbuffer_space_len(serial->txfifo) != 0)
    {
        mask |= POLLOUT;
    }

    return mask;
}

static const struct dfs_file_ops serial_fops =
{
    rt_serial_open,
    rt_serial_close,
    rt_serial_control,
    rt_serial_read,
    rt_serial_write,
    RT_NULL,
    RT_NULL,
    RT_NULL,
    rt_serial_poll,
};

/*
 * serial register
 */
rt_err_t rt_hw_serial_register(rt_serial_t *serial,
                               const char  *name,
                               rt_uint32_t flag,
                               void        *data)
{
    int ret;
    struct rt_device *device;
    RT_ASSERT(serial != RT_NULL);

    device = &(serial->parent);

    device->type = RT_Device_Class_Char;

    device->user_data   = data;


	rt_list_init(&(serial->reader_queue));
	rt_list_init(&(serial->writer_queue));

    /* register a character device */
    ret = rt_device_register(device, name, flag);
    device->fops = &serial_fops;

    return ret;
}

/* ISR for serial interrupt */
void rt_hw_serial_isr(rt_serial_t *serial, int event)
{
    switch (event & 0xff)
    {
    case RT_SERIAL_EVENT_RX_IND:
    {
        int ch = -1;
        uint8_t buf[1];
        /* interrupt mode receive */

        while (1)
        {
            ch = serial->ops->getc(serial);
            if (ch == -1)
                break;

            buf[0] = ch;
            rt_ringbuffer_put(serial->rxfifo, buf, 1);
        }

        rt_wqueue_wakeup(&serial->reader_queue, (void*)POLLIN);
        break;
    }
    case RT_SERIAL_EVENT_TX_DONE:
    {
        uint8_t buf[1];
        size_t size;

        size = rt_ringbuffer_get(serial->txfifo, buf, 1);
        if (size == 0)
        {
            serial->tx_started = 0;
            serial->ops->control(serial, SERIAL_CTRL_TXSTOP, RT_NULL);
        }
        else
        {
            serial->ops->putc(serial, buf[0]);
        }

        rt_wqueue_wakeup(&serial->writer_queue, (void*)POLLOUT);

        break;
    }
    case RT_SERIAL_EVENT_TX_DMADONE:
    {


        break;
    }
    case RT_SERIAL_EVENT_RX_DMADONE:
    {


    }
    break;
    }
}

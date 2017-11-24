#include <rthw.h>
#include <rtdevice.h>
#include <dfs_file.h>
#include <poll.h>

typedef union
{
    int iv;
    short sv;
    char cv;
} adc_value;

static int adc_init(rt_adc_t *adc)
{
    int i;

    adc->samp_freq = 0; /* single covert */
    for (i = 0; i < _ADC_MAXCHN; i ++)
    {
        adc->buf[i] = rt_ringbuffer_create(64);;
    }

    adc->ops->init(adc);
    adc->ops->control(adc, ADC_SAMFREQ_SET, (void*)adc->samp_freq);

    return 0;
}

static int rt_adc_open(struct dfs_fd *fd)
{
    rt_adc_t *adc;
    int ret = 0;

    adc = (rt_adc_t *)fd->dev;
    adc->parent.ref_count ++;
    if (adc->parent.ref_count == 1)
    {
        ret = adc_init(adc);
    }

    fd->data = (void*)((RT_ADC_RD_RAW << 8) | 0); /* current channel 0, read raw data */

    return ret;
}

static int rt_adc_close(struct dfs_fd *fd)
{

    return 0;
}

static int read_raw(rt_adc_t *adc, int chn, rt_bool_t nonb, void *buffer, size_t size)
{
    int rxlen;

    while (1)
    {
        rxlen = rt_ringbuffer_get(adc->buf[chn], buffer, size);

        if (rxlen > 0)
        {
            break;
        }
        else if (nonb)
        {
            rxlen = -EAGAIN;
            break;
        }

        rt_wqueue_wait(&adc->reader_queue, 0, -1);
    }

    return rxlen;
}

static int read_cov(rt_adc_t *adc, int chn, rt_bool_t nonb, void *buffer, size_t size, float coffe)
{
    int rxlen = 0, len;
    adc_value val;
    float cov;
    char *pbuf;

    pbuf = (void*)buffer;

    while (rxlen < size)
    {
        len = rt_ringbuffer_get(adc->buf[chn], (rt_uint8_t*)&val, adc->value_size);

        if (rxlen && (len == 0))
            break;

        if (len > 0)
        {
            switch (len)
            {
            case 1:
                cov = val.cv * coffe;
                break;
            case 2:
                cov = val.sv * coffe;
                break;
            case 4:
                cov = val.iv * coffe;
                break;
            }

            rt_memcpy(pbuf, &cov, sizeof(float));
            rxlen += sizeof(float);
			pbuf += sizeof(float);
            continue;
        }
        else if (nonb)
        {
            if (rxlen == 0)
                rxlen = -EAGAIN;

            break;
        }

        rt_wqueue_wait(&adc->reader_queue, 0, -1);
    }

    return rxlen;
}

static int rt_adc_read(struct dfs_fd *fd, void *buffer, size_t size)
{
    rt_adc_t *adc;
    int chn_num;
    int amode;
    int rxlen;

    adc = (rt_adc_t *)fd->dev;
    chn_num = (int)fd->data&0xFF;
    amode = ((int)fd->data >> 8)&0xFF;

    if (chn_num > _ADC_MAXCHN)
        return 0;

    if (amode != RT_ADC_RD_RAW)
        size &= ~3;

    if (size == 0)
        return 0;

    if (adc->samp_freq == 0)
    {
        adc->ops->control(adc, ADC_START, 0);
    }

    if (amode == RT_ADC_RD_RAW)
    {
        rxlen = read_raw(adc, chn_num, fd->flags & O_NONBLOCK, buffer, size);
    }
    else if (amode == RT_ADC_RD_VOL)
    {
        rxlen = read_cov(adc, chn_num, fd->flags & O_NONBLOCK, buffer, size, adc->coeff_vol);
    }
    else
    {
        rxlen = read_cov(adc, chn_num, fd->flags & O_NONBLOCK, buffer, size, adc->coeff_ratio);
    }

    return rxlen;
}

static int rt_adc_control(struct dfs_fd *fd, int cmd, void *args)
{
    rt_adc_t *adc;
    int ret = -EINVAL;

    adc = (rt_adc_t *)fd->dev;

    switch (cmd & 0xFFFF)
    {
    case ADC_CHN_SEL:
    {
        if ((rt_uint8_t)args < _ADC_MAXCHN)
        {
            fd->data = (void*)((int)args&0xFFFF);
        }
    }
    break;
    case ADC_SAMFREQ_SET:
    {
        if (adc->samp_freq == (int)args)
            break;

        ret = adc->ops->control(adc, cmd, args);
        if (ret == 0 && args != 0)
        {
            ret = adc->ops->control(adc, ADC_START, args);
            adc->samp_freq = (int)args;
        }
    }
    break;
    case ADC_START:
    {
        ret = adc->ops->control(adc, ADC_START, (void*)adc->samp_freq);
    }
    break;
    default:
    {
        ret = adc->ops->control(adc, cmd, args);
    }
    break;
    }

    return ret;
}

static int rt_adc_poll(struct dfs_fd *fd, rt_pollreq_t *req)
{
    int mask = 0;
    rt_adc_t *adc;
    int chn_num;

    adc = (rt_adc_t *)fd->dev;
    chn_num = (int)fd->data&0xFF;

    rt_poll_add(&adc->reader_queue, req);

    if (rt_ringbuffer_space_len(adc->buf[chn_num]) != 0)
    {
        mask |= POLLIN;
    }

    return mask;
}

static const struct dfs_file_ops adc_fops =
{
    rt_adc_open,
    rt_adc_close,
    rt_adc_control,
    rt_adc_read,
    RT_NULL,
    RT_NULL,
    RT_NULL,
    RT_NULL,
    rt_adc_poll,
};

int rt_adc_register(rt_adc_t *adc, const char *name, void *data)
{
    float resolution;

    if (adc->resol <= 8)
    {
        adc->value_size = 1;
    }
    else if (adc->resol <= 16)
    {
        adc->value_size = 2;
    }
    else if (adc->resol <= 32)
    {
        adc->value_size = 4;
    }
    resolution = (1 << (adc->resol + 1)) - 1;
    adc->coeff_ratio = 1/resolution;
    adc->coeff_vol = adc->vref/resolution;

    rt_list_init(&(adc->reader_queue));
    adc->parent.user_data = data;
    rt_device_register(&adc->parent, name, 0);
    adc->parent.fops = &adc_fops;

    return 0;
}

int rt_adc_done(rt_adc_t *adc, rt_uint32_t chn, const void *raw_data, rt_size_t size)
{
    if (chn > _ADC_MAXCHN)
        return -1;

    rt_ringbuffer_put_force(adc->buf[chn], raw_data, size);
    rt_wqueue_wakeup(&adc->reader_queue, (void*)POLLIN);

    return size;
}

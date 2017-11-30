#include <rtdevice.h>

#define NOTALIGNED(x)	((x & (chip->page_size - 1)) != 0)
#define min(a,b) (a>b? b:a)

static const struct nand_oob_region _layout64[3] =
{
    {0, 2},//bad mark
    {2, 38}, //free
    {40, 24} //ecc
};

static int check_offs_len(rt_nand_t *chip, loff_t ofs, size_t len)
{
    int ret = 0;
    uint32_t blkmask;

    blkmask = chip->pages_pb * chip->page_size - 1;
    /* Start address must align on block boundary */
    if (ofs & blkmask)
    {
        ret = -EINVAL;
    }

    /* Length must align on block boundary */
    if (len % blkmask)
    {
        ret = -EINVAL;
    }

    return ret;
}

static uint8_t *nand_fill_oob(rt_mtd_t *mtd, uint8_t *oob, size_t len, struct mtd_oob_ops *ops)
{
    struct nand_chip *chip = mtd->priv;

    /*
     * Initialise to all 0xFF, to avoid the possibility of left over OOB
     * data from a previous OOB read.
     */
    rt_memset(chip->oob_poi, 0xff, chip->oobsize);

    switch (ops->mode)
    {
    case MTD_OPS_PLACE_OOB:
    case MTD_OPS_RAW:
        rt_memcpy(chip->oob_poi + ops->ooboffs, oob, len);
        return oob + len;

    case MTD_OPS_AUTO_OOB:
    {
        const struct nand_oob_region *free = chip->freelayout;
        uint32_t boffs;
        size_t bytes;

        bytes = min(len, free->length);
        boffs = free->offset;

        rt_memcpy(chip->oob_poi + boffs, oob, bytes);
        oob += bytes;

        return oob;
    }
    }

    return NULL;
}

static uint8_t *nand_transfer_oob(rt_nand_t *chip, uint8_t *oob, struct mtd_oob_ops *ops, size_t len)
{
    switch (ops->mode)
    {
    case MTD_OPS_PLACE_OOB:
    case MTD_OPS_RAW:
        rt_memcpy(oob, chip->oob_poi + ops->ooboffs, len);
        return oob + len;

    case MTD_OPS_AUTO_OOB:
    {
        struct nand_oob_region *free = (struct nand_oob_region *)chip->freelayout;
        uint32_t boffs = 0, roffs = ops->ooboffs;
        size_t bytes = 0;

        for (; free->length && len; free++, len -= bytes)
        {
            /* Read request not from offset 0? */
            if (roffs)
            {
                if (roffs >= free->length)
                {
                    roffs -= free->length;
                    continue;
                }
                boffs = free->offset + roffs;
                bytes = min(len,(free->length - roffs));
                roffs = 0;
            }
            else
            {
                bytes = min(len, free->length);
                boffs = free->offset;
            }

            rt_memcpy(oob, chip->oob_poi + boffs, bytes);
            oob += bytes;
        }

        return oob;
    }
    }

    return NULL;
}

static int nand_erase(rt_mtd_t *mtd, struct erase_info *instr)
{
    rt_nand_t *nand;
    int ret = 0;
    int page;
    int pages;

    nand = (rt_nand_t*)mtd->priv;

    if (check_offs_len(nand, instr->addr, instr->len))
        return -EINVAL;

    pages = instr->len/nand->page_size;
    page = instr->addr/nand->page_size;

    while (pages)
    {
        int status;

        status = nand->ops->cmdfunc(nand, NAND_BLK_ERASE, page, 0);
        if (status & NAND_STATUS_FAIL)
        {
            ret = -EIO;
            instr->fail_addr = ((loff_t)page * nand->page_size);
        }

        page += nand->pages_pb;
        pages -= nand->pages_pb;
    }

    return ret;
}

static int nand_read_page_raw(rt_nand_t *chip, uint8_t *buf, int oob_required, int page)
{
    chip->ops->read_buf(chip, buf, chip->page_size);

    if (oob_required)
        chip->ops->read_buf(chip, chip->oob_poi, chip->oobsize);

    return 0;
}

static int nand_do_read_ops(rt_mtd_t *mtd, loff_t from, struct mtd_oob_ops *ops)
{
    int page, bytes;
    char oob_required;
    char ecc_fail = 0;
    struct nand_chip *chip = mtd->priv;
    int ret = 0;
    uint32_t readlen = ops->len;
    uint16_t oobreadlen = ops->ooblen;
    uint16_t max_oobsize = ops->mode == MTD_OPS_AUTO_OOB ?
                           chip->freelayout->length : chip->oobsize;

    uint8_t *oob, *buf;
    uint32_t max_bitflips = 0;

    /* Reject reads, which are not page aligned */
    if (NOTALIGNED(from) || NOTALIGNED(ops->len))
    {
        return -EINVAL;
    }

    page = (int)(from / chip->page_size);
    buf = ops->datbuf;
    oob = ops->oobbuf;
    oob_required = oob ? 1 : 0;

    while (1)
    {
        bytes = min(chip->page_size, readlen);

        chip->ops->cmdfunc(chip, NAND_PAGE_RD, page, 0x00);

        /*
         * Now read the page into the buffer.  Absent an error,
         * the read methods return max bitflips per ecc step.
         */
        if (ops->mode == MTD_OPS_RAW)
        {
            ret = nand_read_page_raw(chip, buf, oob_required, page);
        }
        else
        {
            ret = chip->ecc.read_page(chip, buf, oob_required, page);
        }

        max_bitflips = 0;//max_t(unsigned int, max_bitflips, ret);

        if (oob)
        {
            int toread = min(oobreadlen, max_oobsize);

            if (toread)
            {
                oob = nand_transfer_oob(chip, oob, ops, toread);
                oobreadlen -= toread;
            }
        }

        buf += bytes;
        readlen -= bytes;

        if (!readlen)
            break;

        page++;
    }

    ops->retlen = ops->len - (size_t) readlen;
    if (oob)
        ops->oobretlen = ops->ooblen - oobreadlen;

    if (ret < 0)
        return ret;

    if (ecc_fail)
        return -EBADMSG;

    return max_bitflips;
}

static int nand_read(rt_mtd_t *mtd, loff_t from, size_t len, size_t *retlen, uint8_t *buf)
{
    struct mtd_oob_ops ops;
    int ret;

    rt_memset(&ops, 0, sizeof(ops));
    ops.len = len;
    ops.datbuf = buf;
    ops.mode = MTD_OPS_PLACE_OOB;
    ret = nand_do_read_ops(mtd, from, &ops);
    *retlen = ops.retlen;

    return ret;
}

static int nand_write_page_raw(rt_nand_t *chip, const uint8_t *buf, int oob_required, int page)
{
    chip->ops->write_buf(chip, buf, chip->page_size);

    if (oob_required)
        chip->ops->write_buf(chip, chip->oob_poi, chip->oobsize);

    return 0;
}

static int nand_write_page(rt_nand_t *chip, const uint8_t *buf,
                           int oob_required, int page, int raw)
{
    int status;

    chip->ops->cmdfunc(chip, NAND_PAGE_WR0, page, 0x00);

    if (raw)
    {
        nand_write_page_raw(chip, buf, oob_required, page);
    }
    else
    {
        chip->ecc.write_page(chip, buf, oob_required, page);
    }

    status = chip->ops->cmdfunc(chip, NAND_PAGE_WR1, -1, -1);

    return status;
}


static int nand_do_write_ops(rt_mtd_t *mtd, loff_t to, struct mtd_oob_ops *ops)
{
    int page;
    struct nand_chip *chip = mtd->priv;
    uint16_t writelen = ops->len;
    uint16_t oob_required = ops->oobbuf ? 1 : 0;
    uint16_t oobwritelen = ops->ooblen;
    uint16_t oobmaxlen = ops->mode == MTD_OPS_AUTO_OOB ?
                         chip->freelayout->length : chip->oobsize;

    uint8_t *oob = ops->oobbuf;
    uint8_t *buf = ops->datbuf;
    int ret;

    ops->retlen = 0;
    if (!writelen)
        return 0;

    /* Reject writes, which are not page aligned */
    if (NOTALIGNED(to) || NOTALIGNED(ops->len))
    {
        return -EINVAL;
    }

    page = (int)(to / chip->page_size);

    /* Don't allow multipage oob writes with offset */
    if (oob && ops->ooboffs && (ops->ooboffs + ops->ooblen > oobmaxlen))
    {
        ret = -EINVAL;
        goto err_out;
    }

    while (1)
    {
        uint16_t bytes = chip->page_size;

        if (oob)
        {
            size_t len = min(oobwritelen, oobmaxlen);
            oob = nand_fill_oob(mtd, oob, len, ops);
            oobwritelen -= len;
        }
        else
        {
            /* We still need to erase leftover OOB data */
            rt_memset(chip->oob_poi, 0xff, chip->oobsize);
        }

        ret = nand_write_page(chip, buf, oob_required, page, (ops->mode == MTD_OPS_RAW));
        if (ret)
            break;

        writelen -= bytes;
        if (!writelen)
            break;

        buf += bytes;
        page++;
    }

    ops->retlen = ops->len - writelen;
    if (oob)
        ops->oobretlen = ops->ooblen;

err_out:

    return ret;
}

static int nand_write_page_hwecc(rt_nand_t *chip, const uint8_t *buf, int oob_required, int page)
{
    uint16_t i;
    uint16_t eccsize = chip->ecc.stepsize;
    uint16_t eccbytes = chip->ecc.bytes;
    uint16_t eccsteps = chip->page_size / chip->ecc.stepsize;
    uint16_t eccpos = chip->ecc.layout->offset;
    uint8_t *ecc_calc = chip->buffers.ecccalc;
    const uint8_t *p = buf;

    for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize)
    {
        chip->ops->cmdfunc(chip, NAND_ECC_WRITE, 0, 0);
        chip->ops->write_buf(chip, p, eccsize);
        chip->ecc.calculate(chip, p, &ecc_calc[i]);
    }

    rt_memcpy(&chip->oob_poi[eccpos], ecc_calc, chip->ecc.layout->length);

    chip->ops->write_buf(chip, chip->oob_poi, chip->oobsize);

    return 0;
}

static int nand_read_page_hwecc(rt_nand_t *chip, uint8_t *buf, int oob_required, int page)
{
    uint16_t i;
    uint16_t eccsize = chip->ecc.stepsize;
    uint16_t eccbytes = chip->ecc.bytes;
    uint16_t eccsteps = chip->page_size/chip->ecc.stepsize;
    uint16_t eccpos = chip->ecc.layout->offset;
    uint8_t *p = buf;
    uint8_t *ecc_calc = chip->buffers.ecccalc;
    uint8_t *ecc_code = chip->buffers.ecccode;
    unsigned int max_bitflips = 0;

    for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize)
    {
        chip->ops->cmdfunc(chip, NAND_ECC_READ, 0, 0);
        chip->ops->read_buf(chip, p, eccsize);
        chip->ecc.calculate(chip, p, &ecc_calc[i]);
    }

    chip->ops->read_buf(chip, chip->oob_poi, chip->oobsize);
    rt_memcpy(ecc_code, &chip->oob_poi[eccpos], chip->ecc.layout->length);

    eccsteps = chip->page_size/chip->ecc.stepsize;
    p = buf;

    for (i = 0 ; eccsteps; eccsteps--, i += eccbytes, p += eccsize)
    {
        int stat;

        stat = chip->ecc.correct(chip, p, &ecc_code[i], &ecc_calc[i]);
        if (stat < 0)
        {
            //mtd->ecc_stats.failed++;
        }
        else
        {
            //mtd->ecc_stats.corrected += stat;
            //max_bitflips = max_t(unsigned int, max_bitflips, stat);
        }
    }

    return max_bitflips;
}

static int nand_write(rt_mtd_t *mtd, loff_t to, size_t len, size_t *retlen, const uint8_t *buf)
{
    struct mtd_oob_ops ops;
    int ret;

    rt_memset(&ops, 0, sizeof(ops));
    ops.len = len;
    ops.datbuf = (uint8_t *)buf;
    ops.mode = MTD_OPS_PLACE_OOB;
    ret = nand_do_write_ops(mtd, to, &ops);
    *retlen = ops.retlen;

    return ret;
}

static int nand_read_oob_std(rt_nand_t *chip, int page)
{
    chip->ops->cmdfunc(chip, NAND_PAGE_RD, page, chip->page_size);
    chip->ops->read_buf(chip, chip->oob_poi, chip->oobsize);

    return 0;
}

static int nand_write_oob_std(rt_nand_t *chip, int page)
{
    int status;

    chip->ops->cmdfunc(chip, NAND_PAGE_WR0, page, chip->page_size);
    chip->ops->write_buf(chip, chip->oob_poi, chip->oobsize);
    /* Send command to program the OOB data */
    status = chip->ops->cmdfunc(chip, NAND_PAGE_WR1, -1, -1);

    return status & NAND_STATUS_FAIL ? -EIO : 0;
}

static int nand_do_read_oob(rt_mtd_t *mtd, loff_t from, struct mtd_oob_ops *ops)
{
    int page;
    struct nand_chip *chip = mtd->priv;
    //struct mtd_ecc_stats stats;
    int readlen = ops->ooblen;
    int len;
    uint8_t *buf = ops->oobbuf;
    int ret = 0;

//	stats = mtd->ecc_stats;

    if (ops->mode == MTD_OPS_AUTO_OOB)
        len = chip->freelayout->length;
    else
        len = chip->oobsize;

    if (ops->ooboffs >= len) //attempt to start read outside oob
    {
        return -EINVAL;
    }

    page =  (int)(from / chip->page_size);;

    while (1)
    {
        ret = nand_read_oob_std(chip, page);

        if (ret < 0)
            break;

        len = min(len, readlen);
        buf = nand_transfer_oob(chip, buf, ops, len);

        readlen -= len;
        if (!readlen)
            break;

        /* Increment page address */
        page++;
    }

    ops->oobretlen = ops->ooblen - readlen;

    return ret;
}

/**
 * nand_read_oob - [MTD Interface] NAND read data and/or out-of-band
 * @mtd: MTD device structure
 * @from: offset to read from
 * @ops: oob operation description structure
 *
 *
 */
static int nand_read_oob(rt_mtd_t *mtd, loff_t from, struct mtd_oob_ops *ops)
{
    int ret = -ENOTSUP;

    ops->retlen = 0;

    /* Do not allow reads past end of device */
    if (ops->datbuf && (from + ops->len) > mtd->size)
    {
        return -EINVAL;
    }

    switch (ops->mode)
    {
    case MTD_OPS_PLACE_OOB:
    case MTD_OPS_AUTO_OOB:
    case MTD_OPS_RAW:
        break;

    default:
        goto out;
    }

    if (!ops->datbuf)
        ret = nand_do_read_oob(mtd, from, ops);
    else
        ret = nand_do_read_ops(mtd, from, ops);

out:

    return ret;
}

static int nand_do_write_oob(rt_mtd_t *mtd, loff_t to, struct mtd_oob_ops *ops)
{
    int page, status, len;
    struct nand_chip *chip = mtd->priv;

    if (ops->mode == MTD_OPS_AUTO_OOB)
        len = chip->freelayout->length;
    else
        len = chip->oobsize;

    /* Do not allow write past end of page */
    if ((ops->ooboffs + ops->ooblen) > len)
    {
        return -EINVAL;
    }

    if (ops->ooboffs >= len)
    {
        return -EINVAL;
    }

    /* get page */
    page = (int)(to / chip->page_size);

    nand_fill_oob(mtd, ops->oobbuf, ops->ooblen, ops);

    status = nand_write_oob_std(chip, page);

    ops->oobretlen = ops->ooblen;

    return status;
}

static int nand_write_oob(rt_mtd_t *mtd, loff_t to, struct mtd_oob_ops *ops)
{
    int ret = -ENOTSUP;

    ops->retlen = 0;

    /* Do not allow writes past end of device */
    if (ops->datbuf && (to + ops->len) > mtd->size)
    {
        return -EINVAL;
    }

    switch (ops->mode)
    {
    case MTD_OPS_PLACE_OOB:
    case MTD_OPS_AUTO_OOB:
    case MTD_OPS_RAW:
        break;

    default:
        goto out;
    }

    if (!ops->datbuf)
        ret = nand_do_write_oob(mtd, to, ops);
    else
        ret = nand_do_write_ops(mtd, to, ops);

out:

    return ret;
}

static int nand_block_isbad(rt_mtd_t *mtd, loff_t ofs)
{
    int page, res = 0;
    struct nand_chip *chip = mtd->priv;
    uint16_t bad;

    page = (int)(ofs/chip->page_size);

    chip->ops->cmdfunc(chip, NAND_PAGE_RD, page, chip->page_size);
    chip->ops->read_buf(chip, (uint8_t*)&bad, 2);

    res = (bad != 0xFFFF);

    return res;
}

static int nand_block_markbad(rt_mtd_t *mtd, loff_t ofs)
{
    struct mtd_oob_ops ops;
    uint8_t buf[2] = { 0, 0 };
    int ret ;

    rt_memset(&ops, 0, sizeof(ops));
    ops.oobbuf = buf;
    ops.len = ops.ooblen = 2;
    ops.mode = MTD_OPS_PLACE_OOB;

    ret = nand_do_write_oob(mtd, ofs, &ops);

    return ret;
}

static const struct mtd_ops _ops =
{
    nand_erase,
    nand_read,
    nand_write,
    nand_read_oob,
    nand_write_oob,
    nand_block_isbad,
    nand_block_markbad,
};

int rt_mtd_nand_init(rt_nand_t *nand, int blks_pc, int nchip)
{
    uint8_t *buf;

    buf = rt_malloc(nand->oobsize * 3);
    if (buf == RT_NULL)
        return -ENOMEM;

    nand->oob_poi = buf;
    buf += nand->oobsize;
    nand->buffers.ecccalc = buf;
    buf += nand->oobsize;
    nand->buffers.ecccode = buf;

    nand->size = nand->page_size * nand->pages_pb * blks_pc;
	nand->nchip = nchip;

	nand->mtd.offset = 0;
    nand->mtd.size = nand->size * nchip;
    nand->mtd.parent.type = RT_Device_Class_MTD;
    nand->mtd.ops = &_ops;
    nand->mtd.priv = nand;
    nand->mtd.type = MTD_NANDFLASH;

    nand->freelayout = &_layout64[1];
    nand->ecc.layout = &_layout64[2];

    switch (nand->ecc.mode)
    {
	case NAND_ECC_NONE:
	{
		nand->ecc.read_page = nand_read_page_raw;
		nand->ecc.write_page = nand_write_page_raw;
	}break;
    case NAND_ECC_HW:
    {
        nand->ecc.read_page = nand_read_page_hwecc;
        nand->ecc.write_page = nand_write_page_hwecc;
    }break;
	default:
	{
	    rt_free(buf);
		return -1;
	}
    }

    return 0;
}

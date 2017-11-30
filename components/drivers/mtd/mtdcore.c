#include <rtdevice.h>

rt_mtd_t* mtd_device_get(const char *name)
{
    rt_mtd_t *mtd;

	mtd = (rt_mtd_t *)rt_device_find(name);
	if (mtd == RT_NULL)
		return RT_NULL;

	if (mtd->parent.type != RT_Device_Class_MTD)
		return RT_NULL;

	return mtd;
}

static rt_mtd_t* mtd_part_alloc(rt_mtd_t *master, const rt_mtdpart_t *part)
{
	rt_mtd_t *slave;

	slave = rt_malloc(sizeof(rt_mtd_t));
	if (slave == RT_NULL)
		goto out;

	slave->master = master;

    *slave = *master;
    slave->size = part->length;
    slave->offset = part->offset;
	
out:

	return slave;
}	

int mtd_part_add(rt_mtd_t *master, const rt_mtdpart_t *parts, int n)
{
	int ret;
    rt_mtd_t *slave;

    master->master = master;

	if (n == 1)
	{
        master->offset = parts->offset;
        master->size = parts->length;

        ret = rt_device_register((rt_device_t)master, parts->name, 2);
		n = 0;
	}

    while (n > 0)
	{
        slave = mtd_part_alloc(master, parts);
        ret = rt_device_register((rt_device_t)slave, parts->name, 2);
        if (ret)
			break;
        parts ++;
		n --;
	}

	return ret;
}

int mtd_erase(rt_mtd_t *mtd, struct erase_info *instr)
{
	if (instr->addr >= mtd->size || instr->len > mtd->size - instr->addr)
		return -EINVAL;
    instr->fail_addr = (uint32_t)MTD_FAIL_ADDR_UNKNOWN;
	if (!instr->len)
		return 0;
    
	instr->addr += mtd->offset;

    return mtd->ops->erase(mtd->master, instr);
}

int mtd_read(rt_mtd_t *mtd, loff_t from, size_t len, size_t *retlen, uint8_t *buf)
{
	int ret_code;

	*retlen = 0;
	if (from < 0 || from >= mtd->size || len > mtd->size - from)
		return -EINVAL;

	if (!len)
		return 0;

	/*
	 * In the absence of an error, drivers return a non-negative integer
	 * representing the maximum number of bitflips that were corrected on
	 * any one ecc region (if applicable; zero otherwise).
	 */

	ret_code = mtd->ops->read(mtd->master, from + mtd->offset, len, retlen, buf);

	return ret_code;
}

int mtd_write(rt_mtd_t *mtd, loff_t to, size_t len, size_t *retlen, const uint8_t *buf)
{
	*retlen = 0;
	if (to < 0 || to >= mtd->size || len > mtd->size - to)
		return -EINVAL;
	if (!mtd->ops->write)
		return -EROFS;
	if (!len)
		return 0;

	return mtd->ops->write(mtd->master, to + mtd->offset, len, retlen, buf);
}

int mtd_read_oob(rt_mtd_t *mtd, loff_t from, struct mtd_oob_ops *ops)
{
	int ret_code;

	ops->retlen = ops->oobretlen = 0;
	if (!mtd->ops->read_oob)
		return -EOPNOTSUPP;
	/* Do not allow write past end of device */
	if (from > mtd->size)
		return -EINVAL;

	/*
	 * In cases where ops->datbuf != NULL, mtd->_read_oob() has semantics
	 * similar to mtd->_read(), returning a non-negative integer
	 * representing max bitflips. In other cases, mtd->_read_oob() may
	 * return -EUCLEAN. In all cases, perform similar logic to mtd_read().
	 */
	ret_code = mtd->ops->read_oob(mtd->master, from + mtd->offset, ops);

    return ret_code;
}

int mtd_write_oob(rt_mtd_t *mtd, loff_t to, struct mtd_oob_ops *ops)
{
	ops->retlen = ops->oobretlen = 0;
	if (!mtd->ops->write_oob)
		return -EOPNOTSUPP;
    /* Do not allow write past end of device */
	if (to > mtd->size)
		return -EINVAL;

	return mtd->ops->write_oob(mtd->master, to + mtd->offset, ops);
}

/* Chip-supported device locking */
int mtd_lock(rt_mtd_t *mtd, loff_t ofs, size_t len)
{
    return 0;
}

int mtd_unlock(rt_mtd_t *mtd, loff_t ofs, size_t len)
{
    return 0;
}

int mtd_block_isbad(rt_mtd_t *mtd, loff_t ofs)
{
	if (ofs < 0 || ofs >= mtd->size)
		return -EINVAL;
	if (!mtd->ops->block_isbad)
		return 0;

	return mtd->ops->block_isbad(mtd->master, ofs + mtd->offset);
}

int mtd_block_markbad(rt_mtd_t *mtd, loff_t ofs)
{
	if (!mtd->ops->block_markbad)
		return -EOPNOTSUPP;
	if (ofs < 0 || ofs >= mtd->size)
		return -EINVAL;

	return mtd->ops->block_markbad(mtd->master, ofs + mtd->offset);
}

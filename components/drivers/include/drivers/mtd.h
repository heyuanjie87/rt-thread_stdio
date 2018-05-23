#ifndef __MTD_H__
#define __MTD_H__

#include <stddef.h>
#include "device.h"

#define MTD_NANDFLASH    1


#define MTD_FAIL_ADDR_UNKNOWN    -1

enum
{
	MTD_OPS_PLACE_OOB = 0,
	MTD_OPS_AUTO_OOB = 1,
	MTD_OPS_RAW = 2,
};

struct erase_info;
struct mtd_oob_ops;

#ifndef loff_t
typedef long loff_t;
#endif

struct mtd_info
{
	struct rt_device parent;
    uint8_t type;

	uint32_t size;

	const struct mtd_ops *ops;

	uint32_t offset; //分区起始位置
	struct mtd_info *master; //主分区

	void *priv;
};
typedef struct mtd_info rt_mtd_t;

struct mtd_ops
{
    int (*erase)(rt_mtd_t *mtd, struct erase_info *instr);
    int (*read)(rt_mtd_t *mtd, loff_t from, size_t len, size_t *retlen, uint8_t *buf);
	int (*write)(rt_mtd_t *mtd, loff_t to, size_t len, size_t *retlen, const uint8_t *buf);
	int (*read_oob) (rt_mtd_t *mtd, loff_t from, struct mtd_oob_ops *ops);
	int (*write_oob) (rt_mtd_t *mtd, loff_t to, struct mtd_oob_ops *ops);
	int (*block_isbad) (rt_mtd_t *mtd, loff_t ofs);
	int (*block_markbad) (rt_mtd_t *mtd, loff_t ofs);
};

struct erase_info
{
	uint32_t addr;
	uint32_t len;
	uint32_t fail_addr;
};

struct mtd_oob_ops 
{
	uint8_t	    mode;
	uint8_t		ooblen;
	uint8_t		oobretlen;
	uint8_t	    ooboffs;

	size_t		len;
	size_t		retlen;
	uint8_t		*datbuf;
	uint8_t		*oobbuf;
};

rt_mtd_t* mtd_device_get(const char *name);
int mtd_erase(rt_mtd_t *mtd, struct erase_info *instr);
int mtd_read_oob(rt_mtd_t *mtd, loff_t from, struct mtd_oob_ops *ops);
int mtd_write_oob(rt_mtd_t *mtd, loff_t to, struct mtd_oob_ops *ops);
int mtd_read(rt_mtd_t *mtd, loff_t from, size_t len, size_t *retlen, uint8_t *buf);
int mtd_write(rt_mtd_t *mtd, loff_t to, size_t len, size_t *retlen, const uint8_t *buf);
int mtd_block_markbad(rt_mtd_t *mtd, loff_t ofs);
int mtd_block_isbad(rt_mtd_t *mtd, loff_t ofs);

typedef struct
{
    const char *name;
	loff_t offset;
	size_t length;
}rt_mtdpart_t;

int mtd_part_add(rt_mtd_t *master, const rt_mtdpart_t *parts, int n);

#endif

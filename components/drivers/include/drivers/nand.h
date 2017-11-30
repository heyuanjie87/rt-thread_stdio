#ifndef __NAND_H__
#define __NAND_H__

#include "mtd.h"

/* Status bits */
#define NAND_STATUS_FAIL        0x01
#define NAND_STATUS_FAIL_N1 0x02
#define NAND_STATUS_TRUE_READY  0x20
#define NAND_STATUS_READY   0x40
#define NAND_STATUS_WP          0x80

typedef enum
{
    NAND_PAGE_RD,
    NAND_PAGE_WR0,
    NAND_PAGE_WR1,
    NAND_ECC_READ,
    NAND_ECC_WRITE,
    NAND_BLK_ERASE,
} nand_cmd_t;

typedef enum 
{
    NAND_ECC_NONE,
    NAND_ECC_HW,
} nand_eccmode_t;

/**
 * struct nand_buffers - buffer structure for read/write
 * @ecccalc:    buffer pointer for calculated ECC, size is oobsize.
 * @ecccode:    buffer pointer for ECC read from flash, size is oobsize.
 * @databuf:    buffer pointer for data, size is (page size + oobsize).
 *
 * Do not change the order of buffers. databuf and oobrbuf must be in
 * consecutive order.
 */
struct nand_buffers
{
    uint8_t *ecccalc;
    uint8_t *ecccode;
    uint8_t *databuf;
};

struct nand_oob_region
{
    uint8_t offset;
    uint8_t length;
};

struct nand_chip;

struct nand_ecc
{
    uint8_t mode;
    uint8_t bytes;                /* bytes per ecc step */
    uint16_t stepsize;

    /* driver must set the two interface if HWECC */
    int (*calculate)(struct nand_chip *chip, const uint8_t *dat, uint8_t *ecc_code);
    int (*correct)(struct nand_chip *chip, uint8_t *dat, uint8_t *read_ecc, uint8_t *calc_ecc);

	/* driver do not touch */
    int (*read_page)(struct nand_chip *chip, uint8_t *buf, int oob_required, int page);
    int (*write_page)(struct nand_chip *chip, const uint8_t *buf, int oob_required, int page);
	const struct nand_oob_region *layout;
};

struct nand_chip
{
    rt_mtd_t mtd;

    const struct nand_ops *ops;
    uint16_t pages_pb;
    uint16_t page_size;
    uint16_t oobsize;

    struct nand_ecc ecc;

    struct nand_buffers buffers;
    uint8_t *oob_poi;
    const struct nand_oob_region *freelayout;
    uint32_t size;
    uint8_t nchip;
};
typedef struct nand_chip rt_nand_t;

struct nand_ops
{
    int (*cmdfunc)(rt_nand_t *nand, int cmd, int page, int offset);
    int (*read_buf)(rt_nand_t *nand, uint8_t *buf, int len);
    int (*write_buf)(rt_nand_t *nand, const  uint8_t *buf, int len);
};

int rt_mtd_nand_init(rt_nand_t *nand, int blks_pc, int nchip);

#endif

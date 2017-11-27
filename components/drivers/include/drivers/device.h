#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <rtdef.h>

#define RT_DEVICE_FLAG_RDONLY           0x001           /**< read only */
#define RT_DEVICE_FLAG_WRONLY           0x002           /**< write only */
#define RT_DEVICE_FLAG_RDWR             0x003           /**< read and write */

#define RT_DEVICE_FLAG_REMOVABLE        0x004           /**< removable device */
#define RT_DEVICE_FLAG_ACTIVATED        0x010           /**< device is activated */

/**
 * device (I/O) class type
 */
enum rt_device_class_type
{
    RT_Device_Class_Char = 0,                           /**< character device */
    RT_Device_Class_Block,                              /**< block device */
    RT_Device_Class_NetIf,                              /**< net interface */
    RT_Device_Class_MTD,                                /**< memory device */
    RT_Device_Class_CAN,                                /**< CAN device */
    RT_Device_Class_RTC,                                /**< RTC device */
    RT_Device_Class_Sound,                              /**< Sound device */
    RT_Device_Class_Graphic,                            /**< Graphic device */
    RT_Device_Class_I2CBUS,                             /**< I2C bus device */
    RT_Device_Class_USBDevice,                          /**< USB slave device */
    RT_Device_Class_USBHost,                            /**< USB host bus */
    RT_Device_Class_SPIBUS,                             /**< SPI bus device */
    RT_Device_Class_SPIDevice,                          /**< SPI device */
    RT_Device_Class_SDIO,                               /**< SDIO bus device */
    RT_Device_Class_PM,                                 /**< PM pseudo device */
    RT_Device_Class_Pipe,                               /**< Pipe device */
    RT_Device_Class_Portal,                             /**< Portal device */
    RT_Device_Class_Timer,                              /**< Timer device */
    RT_Device_Class_Miscellaneous,                      /**< Miscellaneous device */
    RT_Device_Class_Unknown                             /**< unknown device */
};

typedef struct rt_device *rt_device_t;
/**
 * Device structure
 */
struct rt_device
{
    struct rt_object          parent;                   /**< inherit from rt_object */

    enum rt_device_class_type type;                     /**< device type */
    rt_uint16_t               flag:15;                     /**< device flag */
	rt_uint16_t               iscon:1;
    rt_uint16_t               open_flag;                /**< device open flag */

    rt_uint8_t                ref_count;                /**< reference count */
    rt_uint8_t                device_id;                /**< 0 - 255 */

    /* common device interface */
    rt_err_t  (*init)   (rt_device_t dev);
    rt_err_t  (*open)   (rt_device_t dev, rt_uint16_t oflag);
    rt_err_t  (*close)  (rt_device_t dev);
    rt_size_t (*read)   (rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size);
    rt_size_t (*write)  (rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size);
    rt_err_t  (*control)(rt_device_t dev, int cmd, void *args);

#if defined(RT_USING_POSIX)
    const struct dfs_file_ops *fops;
    rt_list_t wait_queue;
#endif

    void                     *user_data;                /**< device private data */
};

rt_err_t rt_device_register(rt_device_t dev,
                            const char *name,
                            rt_uint16_t flags);
rt_device_t rt_device_find(const char *name)	;						


#endif

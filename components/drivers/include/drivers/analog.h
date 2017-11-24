#ifndef __ANALOG_H__
#define __ANALOG_H__

#include <rtdevice.h>

#ifndef RT_ADC_MAXCHN
#define _ADC_MAXCHN    1
#else
#define _ADC_MAXCHN    RT_ADC_MAXCHN
#endif

#define RT_ADC_RD_VOL       0
#define RT_ADC_RD_RATIO  1
#define RT_ADC_RD_RAW     2

struct rt_adc_ops;

typedef struct rt_adc_device
{
	struct rt_device parent;
	const struct rt_adc_ops *ops;
    float vref;
	rt_uint8_t resol;
    
	rt_uint8_t value_size;
	float coeff_vol;
	float coeff_ratio;
	int samp_freq;
	rt_ringbuffer_t *buf[_ADC_MAXCHN];
	rt_wqueue_t reader_queue;
}rt_adc_t;

struct rt_adc_ops
{
    int (*init)(rt_adc_t *adc);
	void (*deinit)(rt_adc_t *adc);
	int (*control)(rt_adc_t *adc, int cmd, void *args);
};

int rt_adc_done(rt_adc_t *adc, rt_uint32_t chn, const void *raw_data, rt_size_t size);
int rt_adc_register(rt_adc_t *adc, const char *name, void *data);

#endif

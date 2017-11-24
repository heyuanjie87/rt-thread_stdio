#ifndef WORKQUEUE_H__
#define WORKQUEUE_H__

#include <rtthread.h>

/* workqueue implementation */
struct rt_workqueue
{
    rt_list_t   work_list;
    rt_thread_t work_thread;
};

struct rt_work
{
    rt_list_t list;

    void (*work_func)(struct rt_work* work, void* work_data);
    void *work_data;
};

#ifdef RT_USING_HEAP
/**
 * WorkQueue for DeviceDriver
 */
struct rt_workqueue *rt_workqueue_create(const char* name, rt_uint16_t stack_size, rt_uint8_t priority);
rt_err_t rt_workqueue_destroy(struct rt_workqueue* queue);
rt_err_t rt_workqueue_dowork(struct rt_workqueue* queue, struct rt_work* work);
rt_err_t rt_workqueue_cancel_work(struct rt_workqueue* queue, struct rt_work* work);
void rt_work_init(struct rt_work* work, void (*work_func)(struct rt_work* work, void* work_data),
    void* work_data);

#endif


#endif

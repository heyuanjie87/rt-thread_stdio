#include <stdint.h>

#include <rthw.h>
#include <rtdevice.h>
#include <rtservice.h>

extern struct rt_thread *rt_current_thread;

void rt_wqueue_add(rt_wqueue_t *queue, struct rt_wqueue_node *node)
{
	rt_base_t level;

    node->key = 0;
	level = rt_hw_interrupt_disable();
	rt_list_insert_before(&queue->head, &(node->list));
	rt_hw_interrupt_enable(level);
}

void rt_wqueue_remove(struct rt_wqueue_node *node)
{
	rt_base_t level;

	level = rt_hw_interrupt_disable();
	rt_list_remove(&(node->list));
	rt_hw_interrupt_enable(level);
}

int __wqueue_default_wake(struct rt_wqueue_node *wait, void *key)
{

	return 0;
}

void rt_wqueue_wakeup(rt_wqueue_t *queue, void *key)
{
	rt_base_t level;
	int need_schedule = 0;

	struct rt_list_node *node;
	struct rt_wqueue_node *entry;

	queue->flag = 1;
	if (rt_list_isempty(&queue->head))
        return;

	level = rt_hw_interrupt_disable();
	for (node = queue->head.next; node != &queue->head; node = node->next)
	{
		entry = rt_list_entry(node, struct rt_wqueue_node, list);
		if (entry->wakeup(entry, key) == 0)
		{
			rt_thread_resume(entry->polling_thread);
			need_schedule = 1;

			rt_wqueue_remove(entry);
			break;
		}
	}
	queue->flag = 0;
	rt_hw_interrupt_enable(level);

	if (need_schedule)
		rt_schedule();
}

int rt_wqueue_wait(rt_wqueue_t *queue, struct rt_wqueue_node *wait, int msec)
{
    rt_thread_t tid = rt_current_thread;
    rt_timer_t  tmr = &(tid->thread_timer);
    rt_tick_t tick;
    rt_base_t level;

    tick = rt_tick_from_millisecond(msec);

    if (tick == 0)
        return 0;

    /* current context checking */
    RT_DEBUG_NOT_IN_INTERRUPT;
    if (rt_list_isempty(&wait->list))
			return 0;

    level = rt_hw_interrupt_disable();

	if (!rt_list_isempty(&wait->list))
	{
	    rt_thread_suspend(tid);
        /* start timer */
        if (tick != RT_WAITING_FOREVER)
        {
            rt_timer_control(tmr,
                                        RT_TIMER_CTRL_SET_TIME,
                                        &tick);

            rt_timer_start(tmr);
        }

        rt_hw_interrupt_enable(level);
        rt_schedule();
		rt_wqueue_remove(wait);
	}
	else
	{
	    rt_hw_interrupt_enable(level);
	}

    return 0;
}

void rt_wqueue_wait_init(struct rt_wqueue_node *wait)
{
	wait->polling_thread = rt_thread_self();
	wait->wakeup = __wqueue_default_wake;
	rt_list_init(&wait->list);
}

void rt_wqueue_init(rt_wqueue_t *queue)
{
	rt_list_init(&(queue->head));
	queue->flag = 0;
}

int rt_wqueue_uwait(rt_wqueue_t *queue, int msec)
{
    rt_thread_t tid = rt_current_thread;
    rt_timer_t  tmr = &(tid->thread_timer);
    rt_tick_t tick;
    rt_base_t level;
    struct rt_wqueue_node wait;

    tick = rt_tick_from_millisecond(msec);

    if (tick == 0)
        return 0;

    /* current context checking */
    RT_DEBUG_NOT_IN_INTERRUPT;

	rt_wqueue_wait_init(&wait);
    rt_wqueue_add(queue, &wait);

	if (queue->flag == 0)
	{
		level = rt_hw_interrupt_disable();
	    rt_thread_suspend(tid);
        /* start timer */
        if (tick != RT_WAITING_FOREVER)
        {
            rt_timer_control(tmr,
                                        RT_TIMER_CTRL_SET_TIME,
                                        &tick);

            rt_timer_start(tmr);
        }

        rt_hw_interrupt_enable(level);
        rt_schedule();
		rt_wqueue_remove(&wait);
	}

    return 0;
}

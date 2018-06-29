#ifndef WAITQUEUE_H__
#define WAITQUEUE_H__

#include <rtthread.h>

struct rt_wqueue_node;

typedef struct
{
	rt_list_t head;
    rt_uint16_t flag;
}rt_wqueue_t;

typedef int (*rt_wqueue_func_t)(struct rt_wqueue_node *wait, void *key);

struct rt_wqueue_node
{
	rt_thread_t polling_thread;
	rt_list_t   list;

	rt_wqueue_func_t wakeup;
	rt_uint16_t key;
};
typedef struct rt_wqueue_node rt_wqueue_node_t;

int __wqueue_default_wake(struct rt_wqueue_node *wait, void *key);
void rt_wqueue_init(rt_wqueue_t *queue);
void rt_wqueue_add(rt_wqueue_t *queue, struct rt_wqueue_node *node);
void rt_wqueue_remove(struct rt_wqueue_node *node);
int rt_wqueue_wait(rt_wqueue_t *queue, struct rt_wqueue_node *wait, int msec);
void rt_wqueue_wakeup(rt_wqueue_t *queue, void *key);
void rt_wqueue_wait_init(struct rt_wqueue_node *wait);
int rt_wqueue_uwait(rt_wqueue_t *queue, int msec);

#endif

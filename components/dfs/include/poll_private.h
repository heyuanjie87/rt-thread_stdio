#ifndef POLL_PRIVATE_H
#define POLL_PRIVATE_H

#include <rtdef.h>
#include <rtdevice.h>

struct rt_pollreq;

typedef void (*poll_queue_proc)(rt_wqueue_t *, struct rt_pollreq *);

typedef struct rt_pollreq
{
    poll_queue_proc _proc;
    short _key;
}rt_pollreq_t;

rt_inline void rt_poll_add(rt_wqueue_t *wq, rt_pollreq_t *req)
{
    if (req && req->_proc && wq)
    {
        req->_proc(wq, req);
    }
}

#endif // POLL_PRIVATE_H

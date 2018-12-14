/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author        Notes
 * 2018-12-14     heyuanjie87   first version
 */

/* 
 * lwp run in kernel 
 */

#include "../lwp.h"

typedef int (*rik_entry)(int, char**);

static void lwp_thread(void *p)
{
    rt_thread_t tid;
    struct rt_lwp *lwp;

    lwp = (struct rt_lwp *)p;
    tid = rt_thread_self();
    tid->lwp = lwp;
    ((rik_entry)lwp->text_entry)(lwp->argc, (char**)lwp->args);
    rt_free(lwp->args);
}

static int lwp_argscopy(struct rt_lwp *lwp, const char *args)
{
    int size;
    char *str;
    int i;
    int len;
    int argc = 1;
    char **argv;

    if (!args)
    {
        lwp->argc = 0;
        lwp->args = 0;

        return 0;
    }

    str = rt_strdup(args);
    if (!str)
        return -1;

    len = rt_strlen(args);
    for (i = 0; i < len; i ++)
    {
        if ((args[i] == ' ') && (args[i+1] != ' '))
            argc ++;
    }

    argv = (char**)rt_malloc(argc * sizeof(int));
    if (!argv)
    {
        rt_free(str);
        return -1;
    }

    for (i = 0; i < len; i ++)
    {
        if (str[0] != ' ')
            break;
        else
            str ++;
    }
    for (size = 0, i = 0; size < len; size ++, str ++)
    {
        if (i == 0)
        {
            argv[i] = str;
            i ++;            
        }

        if (str[0] == ' ')
        {
            str[0] = '\0';
            if (str[1] != ' ')
            {
                argv[i] = str + 1;
                i ++;
            }
        }
    }

    lwp->args = (void*)argv;
    lwp->argc = argc;

    return 0;
}

struct rt_lwp* lwp_rik_create(int (*entry)(int, char**))
{
    struct rt_lwp *lwp;

    lwp = rt_calloc(sizeof(*lwp), 1);
    if (lwp)
        lwp->text_entry = (void*)entry;

    return lwp;
}

int lwp_rik_exec(struct rt_lwp *lwp, const char *args)
{
    int ret = -1;
    rt_thread_t tid;

    if (lwp_argscopy(lwp, args) != 0)
        return -1;

    tid = rt_thread_create("klwp", lwp_thread,
                           lwp, 4096, 22, 10);
    if (tid)
        ret = rt_thread_startup(tid);

    if (ret != 0)
    {

    }

    return ret;
}

#include <rtdevice.h>
#include <dfs_posix.h>

#ifdef RT_USING_PIPE
int pipe(int fildes[2])
{
    rt_pipe_t *pipe;
    char dname[8];
    static int pipeno = 0;

    rt_snprintf(dname, 8, "pipe%d", pipeno++);

    pipe = rt_pipe_create(dname);
    if (pipe == RT_NULL)
    {
        return -1;
    }

    fildes[0] = open(dname, O_RDONLY, 0);
    if (fildes[0] < 0)
    {
        return -1;
    }

    fildes[1] = open(dname, O_WRONLY, 0);
    if (fildes[1] < 0)
    {
        close(fildes[1]);
        return -1;
    }

    return 0;
}

int mkfifo(const char *path, mode_t mode)
{
    rt_pipe_t *pipe;
    
    pipe = rt_pipe_create(path);
    if (pipe == RT_NULL)
    {
        return -1;
    }

    return 0;
}
#endif

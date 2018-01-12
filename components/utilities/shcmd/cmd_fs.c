#include <dfs_posix.h>
#include <dfs.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <finsh.h>

#define printcon    printf

static void ls(const char *pathname)
{
    struct stat st;
    char *fullpath, *path;
    DIR *dir;
    struct dirent *dirent;

    fullpath = NULL;
    if (pathname == NULL)
    {
#ifdef DFS_USING_WORKDIR
        /* open current working directory */
        path = rt_strdup(working_directory);
#else
        path = rt_strdup("/");
#endif
        if (path == NULL)
            return; /* out of memory */
    }
    else
    {
        path = (char *)pathname;
    }

    /* list directory */
    dir = opendir(path);
    if (dir != 0)
    {
        printcon("Directory %s:\n", path);
        do
        {
            dirent = readdir(dir);

            if (dirent != 0)
            {
                memset(&stat, 0, sizeof(struct stat));

                /* build full path for each file */
                fullpath = dfs_normalize_path(path, dirent->d_name);
                if (fullpath == NULL)
                    break;

                if (stat(fullpath, &st) == 0)
                {
                    printcon("%-20s", dirent->d_name);
                    if (S_ISDIR(st.st_mode))
                    {
                        printcon("%-25s\n", "<DIR>");
                    }
                    else
                    {
                        printcon("%-25lu\n", st.st_size);
                    }
                }
                else
                    printcon("BAD file: %s\n", dirent->d_name);
                rt_free(fullpath);
            }
        }while(dirent != 0);

        closedir(dir);
    }
    else
    {
        printcon("No such directory\n");
    }

    if (pathname == NULL)
        rt_free(path);
}
FINSH_FUNCTION_EXPORT(ls, List information about the FILEs.);
